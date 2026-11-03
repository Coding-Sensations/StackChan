#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace companion::diagnostic {
enum class Task:uint32_t {Main=0,Codec=1,PairTls=2,AudioTls=3};
enum class StageCode:uint32_t {None=0,Boot=1,TickEnter=2,TickExit=3,NumericCloseEnter=4,NumericCloseExit=5,AudioCloseEnter=6,AudioCloseExit=7,TasksStopEnter=8,TasksStopExit=9,LvglWait=10,LvglAcquired=11,LvglReleased=12,HeartbeatEnter=13,HeartbeatExit=14,Back=15,CodecOpen=16,CodecDecode=17,CodecOutput=18,CodecEncodeEnter=19,CodecEncodeExit=20,TlsReadEnter=21,TlsReadReturn=22,TlsDispatch=23,TlsWriteEnter=24,TlsWriteExit=25,WorkerStart=26,FirstDecode=27,FirstEncode=28,DriverInputOpen=29,DriverInputClose=30,DriverOutputOpen=31,DriverOutputClose=32,DriverRead=33,DriverWrite=34,WorkerStop=35};
enum class FaultCode:uint32_t {None=0,EncoderOpen=1,DecoderOpen=2,ResamplerOpen=3,DecodeError=4,DecodedSize=5,PartialConsumption=6,QueueFull=7,PacketRejected=8,EncodeError=9,InputRead=10,ResamplerBound=11,ResamplerProcess=12,AudioSend=13,DriverInputOpen=14,DriverInputGain=15,DriverInputClose=16,DriverOutputOpen=17,DriverOutputVolume=18,DriverOutputClose=19,DriverRead=20,DriverWrite=21,SdkDriver=22,Write=23,TlsRead=24,TlsWrite=25,HeartbeatWrite=26};
struct Frame {std::array<uint8_t,176> bytes{};std::size_t size=176;};
void Init(uint32_t boot_nonce,uint32_t reset_reason);
uint32_t BootNonce();
void Stage(Task,StageCode);void Sample(Task);void Fault(Task,FaultCode,int32_t detail=0);
void CountTick();void CountHeartbeat(bool success);Frame Snapshot();
}
