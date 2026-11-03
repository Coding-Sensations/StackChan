#pragma once
#include "diagnostics.h"
#include <atomic>
namespace companion::diagnostic {
inline uint32_t Read32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline void Put32(uint8_t* p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
inline uint32_t Crc32(const uint8_t* p,std::size_t n){uint32_t crc=0xffffffff;for(std::size_t i=0;i<n;++i){crc^=p[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320:0);}return crc^0xffffffff;}
struct Record {std::array<uint32_t,18> words;};
struct Retained {uint32_t magic,version;Record record;uint32_t crc;};
inline uint32_t RetainedCrc(const Retained& r){std::array<uint8_t,80> b{};Put32(b.data(),r.magic);Put32(b.data()+4,r.version);for(unsigned i=0;i<18;++i)Put32(b.data()+8+i*4,r.record.words[i]);return Crc32(b.data(),b.size());}
inline bool ValidRecord(const Record& r){
 if(!r.words[0]||r.words[1]>64)return false;
 if(r.words[3]==0){if(r.words[2]!=0xffffffff||r.words[4]||r.words[5])return false;}
 else if(r.words[2]>=4||r.words[3]>26)return false;
 for(unsigned i=0;i<4;++i)if(r.words[6+i*3]>35)return false;
 return true;
}
inline Retained Seal(Record r){Retained s{0x53434452,1,r,0};s.crc=RetainedCrc(s);return s;}
class State {
 struct Slot {std::atomic<uint32_t> sequence{0},point{0},time{0},water{0};};
 std::array<Slot,4> slots_{};
 std::atomic<uint32_t> nonce_{0},reset_{0},ticks_{0},success_{0},failure_{0},dropped_{0},back_{0};
 std::atomic<uint32_t> reason_{0},fault_task_{0xffffffff},detail_{0},fault_time_{0};
 Record prior_{};bool prior_valid_=false;
public:
 void Init(uint32_t nonce,uint32_t reset,const Retained& saved){
  nonce_=nonce?nonce:1;reset_=reset<=64?reset:0;
  prior_valid_=reset!=0&&reset!=1&&reset!=9&&saved.magic==0x53434452&&saved.version==1&&ValidRecord(saved.record)&&saved.record.words[0]!=nonce_.load()&&saved.crc==RetainedCrc(saved);
  prior_={};prior_.words[2]=0xffffffff;if(prior_valid_)prior_=saved.record;
 }
 uint32_t BootNonce()const{return nonce_;}
 bool Active()const{return nonce_.load()!=0;}
 void Mark(Task task,StageCode point,uint32_t now){
  unsigned t=unsigned(task);if(!Active()||t>=4||unsigned(point)>35)return;
  if(task==Task::Main&&point==StageCode::Back)back_=1;
  auto& s=slots_[t];uint32_t seq=s.sequence.load();if((seq&1)||!s.sequence.compare_exchange_strong(seq,seq+1)){++dropped_;return;}
  s.time=now;s.point=unsigned(point);s.sequence.store(seq+2,std::memory_order_release);
 }
 void Sample(Task task,uint32_t water){unsigned t=unsigned(task);if(Active()&&t<4)slots_[t].water=water;}
 void Fail(Task task,FaultCode code,int32_t detail,uint32_t now){
  if(!Active()||unsigned(task)>=4||unsigned(code)==0||unsigned(code)>26)return;
  uint32_t none=0;if(!reason_.compare_exchange_strong(none,0xffffffff))return;
  fault_task_=unsigned(task);detail_=uint32_t(detail);fault_time_=now;reason_.store(unsigned(code),std::memory_order_release);
 }
 void Tick(){if(Active())++ticks_;}void Heartbeat(bool ok){if(Active()){if(ok)++success_;else ++failure_;}}
 Record Current()const{
  Record r{};r.words[0]=nonce_;r.words[1]=reset_;r.words[2]=0xffffffff;
  uint32_t reason=reason_.load(std::memory_order_acquire);if(reason&&reason!=0xffffffff){r.words[2]=fault_task_;r.words[3]=reason;r.words[4]=detail_;r.words[5]=fault_time_;}
  for(unsigned t=0;t<4;++t){const auto& s=slots_[t];for(unsigned attempt=0;attempt<3;++attempt){uint32_t a=s.sequence.load(std::memory_order_acquire);if(a&1)continue;uint32_t p=s.point.load(),time=s.time.load(),water=s.water.load();uint32_t b=s.sequence.load(std::memory_order_acquire);if(a==b){r.words[6+t*3]=p;r.words[7+t*3]=time;r.words[8+t*3]=water;break;}}}
  return r;
 }
 Frame Snapshot(uint32_t now)const{
  Frame f;auto* b=f.bytes.data();b[0]='S';b[1]='C';b[2]='F';b[3]='D';b[4]=1;b[5]=uint8_t(prior_valid_)|(back_.load()?2:0);b[6]=0;b[7]=176;
  auto current=Current();Put32(b+8,current.words[0]);Put32(b+12,current.words[1]);Put32(b+16,now);Put32(b+20,ticks_);Put32(b+24,success_);Put32(b+28,failure_);Put32(b+32,dropped_);
  for(unsigned i=2;i<18;++i){Put32(b+36+(i-2)*4,current.words[i]);}
  for(unsigned i=0;i<18;++i){Put32(b+100+i*4,prior_.words[i]);}
  Put32(b+172,Crc32(b,172));return f;
 }
};
#ifdef ESP_PLATFORM
// ESP-IDF's PSRAM workaround makes the generic trait false. The actual state
// and RTC-save claim are explicitly in internal DRAM: its 32-bit wrappers
// dispatch to hardware S32C1I there, never the external-RAM spinlock fallback.
static_assert(sizeof(std::atomic<uint32_t>)==4 && alignof(std::atomic<uint32_t>)>=4,
              "diagnostics require aligned 32-bit atomics");
#else
static_assert(std::atomic<uint32_t>::is_always_lock_free,"diagnostics require lock-free 32-bit atomics");
#endif
}
