#include "stackchan_sd_ir.h"
#include "sd_mount_diagnostics.h"
#include <apps/instruments/instrument_sd.h>
#include "ir_storage_binding.hpp"
#include "sd_display_lease.hpp"
#include "persistent_state.hpp"
#include <cstdio>
#include <unistd.h>
#include <driver/gpio.h>
#include <driver/sdspi_host.h>
#include <esp_lvgl_port.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <sd_protocol_defs.h>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>

namespace stock_tools {
namespace {
constexpr const char* MountPoint="/sdcard";
constexpr const char* LibraryDirectory="/sdcard/IRLIB";
sdmmc_card_t* card=nullptr;
esp_lcd_panel_io_handle_t panel_io=nullptr;
bool pin_fault=false;
SdDiagnosticState diagnostic;
bool SdCheck(SdStage stage,esp_err_t error){if(error==ESP_OK)return true;diagnostic.Record(stage,error);ESP_LOGE("IR-SD","%s: %s (0x%x)",SdStageText(stage),esp_err_to_name(error),unsigned(error));return false;}
bool Mounted(){return card && !pin_fault;}
class BoardSdDisplayOps final : public SdDisplayOps {
    bool lvgl_locked_=false;
    int dc_level_=0;
public:
    bool Healthy() const override{return !pin_fault;}
    bool Lock() override {
        // At initial mount the LCD/LVGL has not started yet. After binding,
        // lock prevents new LVGL submissions while the SDK queue is drained.
        lvgl_locked_=panel_io!=nullptr;
        const bool ok=!lvgl_locked_ || lvgl_port_lock(5000);
        if(!ok)diagnostic.Record(SdStage::LcdLock,ESP_ERR_TIMEOUT);
        return ok;
    }
    bool DrainLcd() override {
        if(panel_io && !SdCheck(SdStage::LcdDrain,esp_lcd_panel_io_tx_param(panel_io,-1,nullptr,0)))return false;
        dc_level_=gpio_get_level(GPIO_NUM_35);
        return true;
    }
    bool Input() override{return SdCheck(SdStage::MisoInput,gpio_set_direction(GPIO_NUM_35,GPIO_MODE_INPUT));}
    bool Restore() override {
        // Before LCD init GPIO35 belongs to SD input only. After binding restore
        // DC's exact sampled level before enabling output (no output glitch).
        if(!panel_io)return SdCheck(SdStage::MisoInput,gpio_set_direction(GPIO_NUM_35,GPIO_MODE_INPUT));
        const bool level=SdCheck(SdStage::DcLevelRestore,gpio_set_level(GPIO_NUM_35,dc_level_));
        const bool direction=SdCheck(SdStage::DcDirectionRestore,gpio_set_direction(GPIO_NUM_35,GPIO_MODE_INPUT_OUTPUT));
        return level && direction;
    }
    void Unlock() override{if(lvgl_locked_)lvgl_port_unlock();lvgl_locked_=false;}
    void Fault() override{pin_fault=true;diagnostic.Fault();ESP_LOGE("IR-SD","LCD pin restoration failed; display lock retained until restart");}
};
// Every filesystem operation includes stat/access checks, which can themselves
// access the SD bus. Guarding only fopen/read/write would be insufficient.
class SharedSdBankFiles final : public IrBankFiles {
    MountedIrBankFiles files_{LibraryDirectory,Mounted};
    template<class F> bool Guarded(F operation) const {
        BoardSdDisplayOps ops;SdDisplayLease lease(ops);
        if(!lease.Open())return false;
        const bool result=operation();
        return lease.Close() && result;
    }
public:
    bool Available() const override{return Guarded([&]{return files_.Available();});}
    bool Exists(unsigned bank) const override {
        // A guard error must not be interpreted as an empty slot.
        bool result=true;
        if(!Guarded([&]{result=files_.Exists(bank);return true;}))return true;
        return result;
    }
    bool Read(unsigned bank,std::string& bytes,size_t maximum) override{return Guarded([&]{return files_.Read(bank,bytes,maximum);});}
    bool WriteDurable(unsigned bank,std::string_view bytes) override{return Guarded([&]{return files_.WriteDurable(bank,bytes);});}
};
class SharedStateFiles final:public StateBankFiles {
 std::string stem_;size_t maximum_;
 std::string Path(unsigned bank)const{return std::string("/sdcard/STATE/")+stem_+(bank?"_B.DAT":"_A.DAT");}
 template<class F> bool Guarded(F f)const{BoardSdDisplayOps ops;SdDisplayLease lease(ops);if(!lease.Open())return false;bool ok=f();return lease.Close()&&ok;}
public:
 SharedStateFiles(std::string stem,size_t maximum):stem_(std::move(stem)),maximum_(maximum){}
 bool Available()const override{return Guarded([]{struct stat st{};return Mounted()&&stat("/sdcard/STATE",&st)==0&&S_ISDIR(st.st_mode);});}
 std::string ReadIdentityUnlocked()const{
  std::string identity;
   if(!Mounted() || !(card->host.flags&SDMMC_HOST_FLAG_SPI) || card->is_mmc || !card->host.do_transaction)return {};
   // Public host transaction contract, same SEND_CID data/flags as pinned
   // sdmmc_cmd.c; observe raw CID now rather than cached mount-time card->cid.
   sdmmc_response_t raw{};sdmmc_command_t cmd{};cmd.opcode=MMC_SEND_CID;cmd.flags=SCF_CMD_READ|SCF_CMD_ADTC;
   cmd.data=&raw[0];cmd.datalen=sizeof(raw);cmd.timeout_ms=500;
   if(!SdCheck(SdStage::CidRead,card->host.do_transaction(card->host.slot,&cmd)) || !SdCheck(SdStage::CidRead,cmd.error))return {};
   const auto* bytes=reinterpret_cast<const unsigned char*>(raw);char hex[3];
   for(size_t i=0;i<sizeof(raw);++i){std::snprintf(hex,sizeof(hex),"%02x",bytes[i]);identity+=hex;}
  return identity;
 }
 mutable std::string observed_identity_;
 std::string Identity()const override{
  std::string identity;bool ok=Guarded([&]{identity=ReadIdentityUnlocked();return !identity.empty();});
  if(ok){observed_identity_=identity;}
  return ok?identity:std::string{};
 }
 bool Exists(unsigned bank)const override{bool exists=true;if(bank>1)return true;if(!Guarded([&]{if(!Mounted())return false;struct stat st{};exists=stat(Path(bank).c_str(),&st)==0||errno!=ENOENT;return true;}))return true;return exists;}
 bool Read(unsigned bank,std::string& out,size_t maximum)override{return bank<2&&maximum<=maximum_&&Guarded([&]{if(!Mounted())return false;FILE* f=fopen(Path(bank).c_str(),"rb");if(!f)return false;std::string next;char block[256];bool ok=true;for(;;){size_t n=fread(block,1,sizeof(block),f);if(next.size()+n>maximum){ok=false;break;}next.append(block,n);if(n<sizeof(block)){ok=!ferror(f);break;}}if(fclose(f)!=0)ok=false;if(ok)out=std::move(next);return ok;});}
 bool WriteDurable(unsigned bank,std::string_view data)override{return bank<2&&data.size()<=maximum_&&Guarded([&]{if(!Mounted() || observed_identity_.empty() || ReadIdentityUnlocked()!=observed_identity_)return false;FILE* f=fopen(Path(bank).c_str(),"wb");if(!f)return false;bool ok=fwrite(data.data(),1,data.size(),f)==data.size();if(ok)ok=fflush(f)==0;if(ok)ok=fsync(fileno(f))==0;if(fclose(f)!=0)ok=false;return ok;});}
};
} // namespace
bool MountStackChanIrSd() {
    if(card || panel_io || pin_fault){diagnostic.Record(SdStage::Precondition,ESP_ERR_INVALID_STATE);return false;}
    diagnostic.Attempt();
    // LCD CS is inactive while the card enters SPI mode; both devices share
    // SCK/MOSI. These are the manufacturer's factory StackChan/CoreS3 pins.
    if(!SdCheck(SdStage::LcdCsLevel,gpio_set_level(GPIO_NUM_3,1)) || !SdCheck(SdStage::LcdCsDirection,gpio_set_direction(GPIO_NUM_3,GPIO_MODE_OUTPUT)) ||
       !SdCheck(SdStage::SdCsLevel,gpio_set_level(GPIO_NUM_4,1)) || !SdCheck(SdStage::SdCsDirection,gpio_set_direction(GPIO_NUM_4,GPIO_MODE_OUTPUT)))return false;
    sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=SPI3_HOST;
    sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();slot.host_id=SPI3_HOST;slot.gpio_cs=GPIO_NUM_4;
    esp_vfs_fat_sdmmc_mount_config_t config{};
    config.format_if_mount_failed=false;config.max_files=4;config.allocation_unit_size=16384;
    if(!SdCheck(SdStage::CardOrFilesystemMount,DiagnosedSdMount(MountPoint,&host,&slot,&config,&card))){card=nullptr;return false;}
    diagnostic.Mounted(true);
    if(mkdir(LibraryDirectory,0700)!=0 && errno!=EEXIST){diagnostic.Record(SdStage::IrDirectory,0,errno);diagnostic.Mounted(false);esp_vfs_fat_sdcard_unmount(MountPoint,card);card=nullptr;return false;}
    if(mkdir("/sdcard/STATE",0700)!=0 && errno!=EEXIST){diagnostic.Record(SdStage::StateDirectory,0,errno);diagnostic.Mounted(false);esp_vfs_fat_sdcard_unmount(MountPoint,card);card=nullptr;return false;}
    if(!ConfigureIrBankFiles(std::make_unique<SharedSdBankFiles>())){
        if(diagnostic.Read().stage==SdStage::NotAttempted)diagnostic.Record(SdStage::IrBinding,ESP_FAIL);
        diagnostic.Mounted(false);
        esp_vfs_fat_sdcard_unmount(MountPoint,card);card=nullptr;return false;
    }
    diagnostic.Record(SdStage::Ready);
    return true;
}
SdDiagnostic ReadStackChanSdDiagnostic(){return diagnostic.Read();}
const char* StackChanSdUnavailableText(){static char text[96];FormatSdDiagnostic(diagnostic.Read(),text,sizeof(text));return text;}
std::unique_ptr<StateBankFiles> MakeStackChanStateFiles(const char* stem,size_t maximum){
 if(!stem || (std::string(stem)!="PET" && std::string(stem)!="WOL" && std::string(stem)!="MUS") || maximum>1152)return {};
 return std::make_unique<SharedStateFiles>(stem,maximum);
}
bool ReadStackChanInstrumentSample(unsigned sample_id,std::string& output){
 static constexpr const char* names[]={"KICK.WAV","SNARE.WAV","HAT.WAV","CLAP.WAV","TOM.WAV","RIM.WAV","SHKR.WAV","CYMBAL.WAV"};
 if(sample_id>=8 || !Mounted())return false;
 BoardSdDisplayOps ops;SdDisplayLease lease(ops);if(!lease.Open())return false;
 SharedStateFiles identity_files("MUS",1152);auto identity=identity_files.ReadIdentityUnlocked();
 std::string next;bool ok=!identity.empty();
 if(ok){
  auto path=std::string("/sdcard/MUSIC/SAMPLES/")+names[sample_id];FILE* file=fopen(path.c_str(),"rb");
  if(!file)ok=false;
  else{
   char block[512];for(;;){auto count=fread(block,1,sizeof(block),file);if(next.size()+count>98304){ok=false;break;}next.append(block,count);if(count<sizeof(block)){ok=!ferror(file);break;}}
   if(fclose(file)!=0)ok=false;
   if(identity_files.ReadIdentityUnlocked()!=identity)ok=false;
  }
 }
 const bool closed=lease.Close();if(ok&&closed){output=std::move(next);return true;}return false;
}
void BindStackChanIrLcd(esp_lcd_panel_io_handle_t io){panel_io=io;diagnostic.Bound(io!=nullptr);}
} // namespace stock_tools
