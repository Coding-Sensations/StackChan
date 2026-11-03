#include "sd_mount_diagnostics.h"
#include <ff.h>
#include <atomic>
namespace {
using Transaction=esp_err_t(*)(int,sdmmc_command_t*);
struct Probe {stock_tools::SdBootTrace trace;Transaction original=nullptr;};
thread_local Probe* active=nullptr;
stock_tools::SdBootTrace cached;stock_tools::SdElectricalTrace electrical;
std::atomic<bool> published{false};
esp_err_t ForwardTransaction(int slot,sdmmc_command_t* command){
 // Only this synchronous boot mount adopts the wrapper; outside it, delegate
 // the exact supported SPI transaction. Published card callback is restored.
 if(!active)return sdspi_host_do_transaction(slot,command);
 auto& probe=*active;stock_tools::SdCommandTrace observed;
 if(command)observed={true,command->opcode,command->timeout_ms,++probe.trace.transactions,0,0};
 const auto result=probe.original(slot,command);
 if(command){observed.transport_error=result;observed.command_error=command->error;probe.trace.last=observed;if(result!=ESP_OK||command->error!=ESP_OK)probe.trace.last_failed=observed;}
 return result;
}
}
extern "C" esp_err_t __real_sdmmc_card_init(const sdmmc_host_t*,sdmmc_card_t*);
extern "C" FRESULT __real_f_mount(FATFS*,const TCHAR*,BYTE);
extern "C" esp_err_t __wrap_sdmmc_card_init(const sdmmc_host_t* host,sdmmc_card_t* card){
 if(!active||!host||!host->do_transaction)return __real_sdmmc_card_init(host,card);
 auto& probe=*active;probe.trace.phase=stock_tools::SdMountPhase::CardInit;probe.trace.init_seen=true;
 if(!(host->flags&SDMMC_HOST_FLAG_SPI)||host->do_transaction!=sdspi_host_do_transaction){const auto result=__real_sdmmc_card_init(host,card);probe.trace.init_result=result;return result;}
 probe.original=host->do_transaction;auto forwarded=*host;forwarded.do_transaction=ForwardTransaction;
 const auto result=__real_sdmmc_card_init(&forwarded,card);probe.trace.init_result=result;return result;
}
extern "C" FRESULT __wrap_f_mount(FATFS* fs,const TCHAR* path,BYTE option){
 if(!active||!fs||option!=1)return __real_f_mount(fs,path,option);
 auto& probe=*active;probe.trace.phase=stock_tools::SdMountPhase::FatMount;probe.trace.fat_seen=true;
 const auto result=__real_f_mount(fs,path,option);probe.trace.fat_result=static_cast<int32_t>(result);return result;
}
namespace stock_tools {
SdBootTrace ReadSdBootTrace(){return published.load(std::memory_order_acquire)?cached:SdBootTrace{};}
void RecordSdElectrical(int32_t switch_error,uint8_t p0,int32_t control_error,uint8_t control,int32_t voltage_error,uint8_t voltage){electrical={true,switch_error,control_error,voltage_error,p0,control,voltage};}
esp_err_t DiagnosedSdMount(const char* path,const sdmmc_host_t* host,const sdspi_device_config_t* slot,const esp_vfs_fat_sdmmc_mount_config_t* config,sdmmc_card_t** card){
 // Nested/foreign invocations keep original semantics and receive no new probe.
 if(active)return esp_vfs_fat_sdspi_mount(path,host,slot,config,card);
 Probe probe;probe.trace.phase=SdMountPhase::Setup;probe.trace.electrical=electrical;published=false;active=&probe;
 const auto result=esp_vfs_fat_sdspi_mount(path,host,slot,config,card);
 if(result==ESP_OK&&card&&*card&&probe.original)(*card)->host.do_transaction=probe.original;
 probe.trace.mount_result=result;if(result==ESP_OK)probe.trace.phase=SdMountPhase::Complete;
 active=nullptr;cached=probe.trace;published.store(true,std::memory_order_release);return result;
}
}
