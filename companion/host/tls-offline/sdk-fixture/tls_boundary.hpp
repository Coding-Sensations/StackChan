#pragma once
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include "companion/diagnostics.h"
namespace tls_fixture {
inline std::atomic<int> inside{0},max_inside{0},writes{0},reads{0},destroyed{0},retries{0};
inline std::mutex gate;inline std::condition_variable changed;
inline bool hold_read=false,release_read=false,entered_read=false;
inline std::atomic<int> read_want_write{0};inline bool closed_fd=false;
inline std::atomic<bool> deliver{false},delivered{false};inline bool invalid_fd=false;
inline thread_local void* task=reinterpret_cast<void*>(1);
struct Event {std::mutex mutex;std::condition_variable changed;unsigned bits=0;};
inline std::thread receiver;inline int last_fd=-1;inline bool nonblocking_at_task_start=false;
inline void enter(){int n=++inside;int old=max_inside.load();while(old<n&&!max_inside.compare_exchange_weak(old,n)){};}
inline void leave(){--inside;}
}
using TaskHandle_t=void*;using EventGroupHandle_t=tls_fixture::Event*;
constexpr int pdPASS=1,pdFALSE=0,portMAX_DELAY=-1,BIT0=1,ESP_OK=0;
#define pdMS_TO_TICKS(x) (x)
#define configASSERT(x) assert(x)
#define ESP_LOGE(tag,...) ((void)(tag))
inline TaskHandle_t xTaskGetCurrentTaskHandle(){return tls_fixture::task;}
inline int xTaskCreate(void(*entry)(void*),const char*,int,void* argument,int,TaskHandle_t* handle){
 tls_fixture::nonblocking_at_task_start=(fcntl(tls_fixture::last_fd,F_GETFL,0)&O_NONBLOCK)!=0;
 *handle=reinterpret_cast<void*>(2);tls_fixture::receiver=std::thread([=]{tls_fixture::task=reinterpret_cast<void*>(2);entry(argument);});return pdPASS;
}
inline void vTaskDelete(void*){}
inline void vTaskDelay(unsigned ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
inline EventGroupHandle_t xEventGroupCreate(){return new tls_fixture::Event;}
inline void vEventGroupDelete(EventGroupHandle_t e){delete e;}
inline void xEventGroupClearBits(EventGroupHandle_t e,unsigned bits){std::lock_guard<std::mutex> guard(e->mutex);e->bits&=~bits;}
inline void xEventGroupSetBits(EventGroupHandle_t e,unsigned bits){{std::lock_guard<std::mutex> guard(e->mutex);e->bits|=bits;}e->changed.notify_all();}
inline unsigned xEventGroupWaitBits(EventGroupHandle_t e,unsigned bits,int,int,int){
 {std::unique_lock<std::mutex> guard(e->mutex);e->changed.wait(guard,[=]{return(e->bits&bits)==bits;});}
 if(tls_fixture::receiver.joinable())tls_fixture::receiver.join();return bits;
}
inline int64_t esp_timer_get_time(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct esp_tls_t {int fd=-1,other=-1;};
struct esp_tls_cfg_t {const unsigned char* cacert_buf=nullptr;unsigned cacert_bytes=0;const char* common_name=nullptr;bool skip_common_name=true;void* crt_bundle_attach=nullptr;int timeout_ms=0;};
using esp_tls_error_handle_t=void*;using esp_err_t=int;
constexpr int ESP_TLS_ERR_SSL_WANT_READ=-0x6900,ESP_TLS_ERR_SSL_WANT_WRITE=-0x6880;
inline esp_tls_t* esp_tls_init(){return new esp_tls_t;}
inline int esp_tls_conn_new_sync(const char*,std::size_t,int,const esp_tls_cfg_t*,esp_tls_t* t){int f[2];assert(pipe(f)==0);t->fd=f[0];t->other=f[1];tls_fixture::last_fd=f[0];if(tls_fixture::closed_fd){close(t->fd);t->fd=-1;tls_fixture::last_fd=-1;}return 1;}
inline int esp_tls_get_conn_sockfd(esp_tls_t* t,int* f){*f=tls_fixture::invalid_fd?-1:(tls_fixture::closed_fd?999999:t->fd);return ESP_OK;}
inline int esp_tls_conn_destroy(esp_tls_t* t){assert(tls_fixture::inside==0);close(t->fd);close(t->other);delete t;++tls_fixture::destroyed;return 0;}
inline int esp_tls_get_error_handle(esp_tls_t*,esp_tls_error_handle_t*){return -1;}
inline int esp_tls_get_and_clear_last_error(esp_tls_error_handle_t,int*,int*){return -1;}
inline int esp_tls_conn_read(esp_tls_t*,char* data,std::size_t){
 tls_fixture::enter();++tls_fixture::reads;
 {std::unique_lock<std::mutex> guard(tls_fixture::gate);if(tls_fixture::hold_read&&!tls_fixture::release_read){tls_fixture::entered_read=true;tls_fixture::changed.notify_all();tls_fixture::changed.wait(guard,[]{return tls_fixture::release_read;});}}
 int result=ESP_TLS_ERR_SSL_WANT_READ;
 if(tls_fixture::read_want_write>0){--tls_fixture::read_want_write;result=ESP_TLS_ERR_SSL_WANT_WRITE;}
 if(tls_fixture::deliver&&!tls_fixture::delivered.exchange(true)){data[0]='P';result=1;}
 tls_fixture::leave();return result;
}
inline int esp_tls_conn_write(esp_tls_t*,const char*,std::size_t n){tls_fixture::enter();++tls_fixture::writes;tls_fixture::changed.notify_all();int left=tls_fixture::retries.load();int result=int(n);if(left>0){--tls_fixture::retries;result=left==2?ESP_TLS_ERR_SSL_WANT_READ:ESP_TLS_ERR_SSL_WANT_WRITE;}tls_fixture::leave();return result;}
namespace companion::diagnostic {inline void Stage(Task,StageCode){}inline void Sample(Task){}inline void Fault(Task,FaultCode,int32_t){}}
