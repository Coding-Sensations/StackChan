#include <companion/diagnostic_state.hpp>
#include <cassert>
#include <thread>
#include <vector>
using namespace companion::diagnostic;
int main(){
 State state;Retained empty{};state.Init(123,15,empty);
 state.Mark(Task::Main,StageCode::TickEnter,10);state.Sample(Task::Main,2048);
 state.Fail(Task::Codec,FaultCode::DecodeError,-7,11);state.Fail(Task::Main,FaultCode::Write,-9,12);
 auto current=state.Current();assert(current.words[0]==123&&current.words[2]==1&&current.words[3]==4&&current.words[4]==uint32_t(-7)&&current.words[5]==11);
 assert(current.words[6]==2&&current.words[7]==10&&current.words[8]==2048);
 Retained saved=Seal(current);State next;next.Init(456,15,saved);auto frame=next.Snapshot(99);assert(frame.bytes[5]==1&&Read32(frame.bytes.data()+100)==123);
 State power;power.Init(789,1,saved);assert(power.Snapshot(99).bytes[5]==0);
 saved.crc^=1;State corrupt;corrupt.Init(790,15,saved);assert(corrupt.Snapshot(99).bytes[5]==0);
 state.Mark(Task::Codec,StageCode::Back,12);assert(!(state.Snapshot(20).bytes[5]&2));
 state.Mark(Task::Main,StageCode::Back,13);state.Mark(Task::Main,StageCode::AudioCloseEnter,14);state.Mark(Task::Main,StageCode::TickExit,15);assert(state.Snapshot(20).bytes[5]&2);
 auto f=state.Snapshot(20);assert(f.size==176&&Read32(f.bytes.data()+172)==Crc32(f.bytes.data(),172));
 Retained invalid=Seal(current);invalid.record.words[3]=999;invalid=Seal(invalid.record);State invalid_state;invalid_state.Init(999,15,invalid);assert(invalid_state.Snapshot(99).bytes[5]==0);
 State raced;raced.Init(345,15,empty);std::atomic<bool> go{false};std::vector<std::thread> claimers;
 for(int i=0;i<4;++i)claimers.emplace_back([&,i]{while(!go.load())std::this_thread::yield();raced.Fail(Task(i),FaultCode::Write,-100-i,77);});go=true;for(auto& t:claimers)t.join();
 auto first=raced.Current();assert(first.words[2]<4&&first.words[3]==23&&first.words[4]==uint32_t(-100-int(first.words[2]))&&first.words[5]==77);
 std::vector<std::thread> threads;for(int i=0;i<4;++i)threads.emplace_back([&,i]{for(int n=0;n<1000;++n){state.Mark(Task(i),StageCode::TickExit,n);state.Sample(Task(i),1024);state.Fail(Task(i),FaultCode::Write,-1,n);state.Tick();state.Heartbeat(true);}});
 for(auto& t:threads)t.join();assert(state.Current().words[3]==4);assert(Read32(state.Snapshot(2000).bytes.data()+20)==4000);
}
