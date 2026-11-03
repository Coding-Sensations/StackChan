#pragma once
#include <cstdint>
namespace stock_tools {
enum class SdMountPhase:uint8_t {Unknown,Setup,CardInit,FatMount,Complete};
struct SdCommandTrace {bool seen=false;uint32_t opcode=0,timeout_ms=0,ordinal=0;int32_t transport_error=0,command_error=0;};
struct SdElectricalTrace {bool sampled=false;int32_t switch_error=0,control_error=0,voltage_error=0;uint8_t p0=0,control=0,voltage=0;};
struct SdBootTrace {
 SdMountPhase phase=SdMountPhase::Unknown;bool init_seen=false,fat_seen=false;
 int32_t init_result=0,fat_result=0,mount_result=0;uint32_t transactions=0;
 SdCommandTrace last,last_failed;SdElectricalTrace electrical;
};
SdBootTrace ReadSdBootTrace();
void RecordSdElectrical(int32_t switch_error,uint8_t p0,int32_t control_error,uint8_t control,int32_t voltage_error,uint8_t voltage);
inline const char* SdPhaseText(SdMountPhase phase){switch(phase){case SdMountPhase::Unknown:return "not observed";case SdMountPhase::Setup:return "host/slot setup";case SdMountPhase::CardInit:return "card initialization";case SdMountPhase::FatMount:return "FAT mount";case SdMountPhase::Complete:return "complete";}return "unknown";}
}
