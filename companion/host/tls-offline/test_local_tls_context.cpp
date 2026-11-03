// Executes production LocalTls with finite SDK boundaries; no network/device IO.
#include "companion/local_tls.h"
#include "tls_boundary.hpp"
#include <cstdlib>
#include <cstdio>
extern "C" const char* companion_local_ca_pem(){return "CA";}
extern "C" const char* companion_local_tls_host(){return "192.168.1.12";}
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
int main(int argc,char** argv){
 require(argc==2,"mode");const std::string mode=argv[1];
 if(mode=="invalid_fd"||mode=="closed_fd"){tls_fixture::invalid_fd=mode=="invalid_fd";tls_fixture::closed_fd=mode=="closed_fd";companion::LocalTls transport;require(!transport.Connect("192.168.1.12",18443),"unusable socket fails before receiver starts");require(!tls_fixture::receiver.joinable()&&tls_fixture::destroyed==1,"failed nonblocking setup cleans context");return 0;}
 if(mode=="overlap")tls_fixture::hold_read=true;
 companion::LocalTls transport;
 if(mode=="read_retry")tls_fixture::read_want_write=1;
 if(mode=="callback_stop")transport.OnStream([&](const std::string&){transport.Disconnect();});
 if(mode=="callback")transport.OnStream([&](const std::string& data){require(data=="P","dispatch bytes");require(transport.Send("pong")==4,"callback writer runs outside context lock");});
 require(transport.Connect("192.168.1.12",18443),"connect");
 require(tls_fixture::nonblocking_at_task_start,"socket nonblocking before receive task starts");
 if(mode=="overlap"){
  {std::unique_lock<std::mutex> guard(tls_fixture::gate);require(tls_fixture::changed.wait_for(guard,std::chrono::seconds(1),[]{return tls_fixture::entered_read;}),"read entered");}
  std::atomic<bool> started{false};std::thread writer([&]{started=true;require(transport.Send("heartbeat")==9,"writer progresses");});
  while(!started)std::this_thread::yield();
  {std::unique_lock<std::mutex> guard(tls_fixture::gate);tls_fixture::changed.wait_for(guard,std::chrono::milliseconds(50),[]{return tls_fixture::writes>0;});tls_fixture::release_read=true;}tls_fixture::changed.notify_all();writer.join();
  transport.Disconnect();require(tls_fixture::max_inside==1,"read/write never overlap on one TLS context");
 }else if(mode=="callback"){
  tls_fixture::deliver=true;auto until=std::chrono::steady_clock::now()+std::chrono::seconds(1);while(tls_fixture::writes==0&&std::chrono::steady_clock::now()<until)std::this_thread::yield();transport.Disconnect();require(tls_fixture::writes==1,"callback pong progresses");
 }else if(mode=="callback_stop"){
  tls_fixture::deliver=true;auto until=std::chrono::steady_clock::now()+std::chrono::seconds(1);while(!tls_fixture::delivered&&std::chrono::steady_clock::now()<until)std::this_thread::yield();transport.Disconnect();require(tls_fixture::delivered,"receive callback stop returns without self join");
 }else if(mode=="read_retry"){
  auto until=std::chrono::steady_clock::now()+std::chrono::seconds(1);while(tls_fixture::reads<2&&std::chrono::steady_clock::now()<until)std::this_thread::yield();require(tls_fixture::reads>=2&&transport.Send("heartbeat")==9,"read WANT_WRITE is retried and does not terminate transport");transport.Disconnect();
 }else{
  require(mode=="idle","known mode");tls_fixture::retries=2;auto begin=std::chrono::steady_clock::now();require(transport.Send("heartbeat")==9,"idle read permits bounded heartbeat write and both WANT retries");require(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(500),"idle transport does not starve writes");transport.Disconnect();require(tls_fixture::writes==3,"same write retries to completion");
 }
 require(tls_fixture::destroyed==1&&tls_fixture::inside==0&&!tls_fixture::receiver.joinable(),"join and drain before destroy");std::puts("production TLS context test PASS");
}
