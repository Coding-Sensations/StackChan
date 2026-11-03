#pragma once
#include <atomic>
#include "sd_boot_trace.hpp"
#include <cstdint>
#include <cstdio>
namespace stock_tools {
enum class SdStage:uint8_t {NotAttempted,Precondition,LcdCsLevel,LcdCsDirection,SdCsLevel,SdCsDirection,CardOrFilesystemMount,IrDirectory,StateDirectory,IrBinding,Ready,LcdLock,LcdDrain,MisoInput,DcLevelRestore,DcDirectionRestore,CidRead};
struct SdDiagnostic {SdStage stage;int32_t code;int filesystem_errno;bool mount_attempted,mounted,display_bound,display_fault;SdBootTrace boot;};
class SdDiagnosticState {
 std::atomic<SdStage> stage_{SdStage::NotAttempted};std::atomic<int32_t> code_{0};std::atomic<int> errno_{0};
 std::atomic<bool> attempted_{false},mounted_{false},bound_{false},fault_{false};
public:
 void Attempt(){attempted_=true;}
 void Mounted(bool value){mounted_=value;}
 void Bound(bool value){bound_=value;}
 void Fault(){fault_=true;}
 void Record(SdStage stage,int32_t code=0,int fs_errno=0){code_=code;errno_=fs_errno;stage_=stage;}
 SdDiagnostic Read()const{return {stage_.load(),code_.load(),errno_.load(),attempted_.load(),mounted_.load(),bound_.load(),fault_.load(),ReadSdBootTrace()};}
};
inline const char* SdStageText(SdStage stage){switch(stage){
 case SdStage::NotAttempted:return "SD initialization not attempted";
 case SdStage::Precondition:return "SD initialization refused";
 case SdStage::LcdCsLevel:case SdStage::LcdCsDirection:return "LCD chip-select setup failed";
 case SdStage::SdCsLevel:case SdStage::SdCsDirection:return "SD chip-select setup failed";
 case SdStage::CardOrFilesystemMount:return "SD card/filesystem mount failed";
 case SdStage::IrDirectory:return "SD IR directory unavailable";
 case SdStage::StateDirectory:return "SD state directory unavailable";
 case SdStage::IrBinding:return "SD storage binding failed";
 case SdStage::Ready:return "SD mounted";
 case SdStage::LcdLock:return "SD display lock unavailable";
 case SdStage::LcdDrain:return "SD LCD queue drain failed";
 case SdStage::MisoInput:return "SD MISO pin setup failed";
 case SdStage::DcLevelRestore:case SdStage::DcDirectionRestore:return "SD display restore failed; restart";
 case SdStage::CidRead:return "SD card identity read failed";
 }return "SD status unknown";}
inline void FormatSdDiagnostic(const SdDiagnostic& d,char* output,size_t size){
 if(d.filesystem_errno)std::snprintf(output,size,"%s (errno %d)",SdStageText(d.stage),d.filesystem_errno);
 else if(d.code)std::snprintf(output,size,"%s (0x%lx)",SdStageText(d.stage),static_cast<unsigned long>(static_cast<uint32_t>(d.code)));
 else std::snprintf(output,size,"%s",SdStageText(d.stage));
}
}
