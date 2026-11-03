from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent
source=(r.parent/'main/companion/muse_voice_task.cc').read_text()
def method(signature):
 start=source.index(signature);begin=source.index('{',start);depth=1;end=begin+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
fixture=r'''
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <cassert>
#include <cstdio>
struct Exit{};static bool resources=false;static unsigned creates=0,deletes=0;static bool create_ok=false;static void(*entry)(void*)=nullptr;static void* context=nullptr;static std::function<void()> during_delay;
constexpr int pdPASS=1;unsigned pdMS_TO_TICKS(unsigned value){return value;}
void vTaskDelay(unsigned ms){assert(ms==20);if(during_delay)during_delay();}
struct Task {bool ready=false,retired=false;unsigned stops=0,starts=0;bool Start(unsigned stack){assert(stack==12288);++starts;return true;}void SignalStop(){++stops;}bool TryRetire(){retired=ready;return ready;}};
enum class VoiceTaskStage{Prepared,Starting};
struct Window{std::atomic<bool> creation_finished{false},revoked{true},retiring{false},reaping{false},started{false};std::atomic<VoiceTaskStage> stage{VoiceTaskStage::Prepared};std::function<bool()> permission;bool Current(){return permission?permission():true;}std::mutex reap_mutex;std::unique_ptr<Task> task=std::make_unique<Task>();void ReleaseOriginal();};
std::mutex mutex;std::shared_ptr<Window> window;std::weak_ptr<Window> weak;uint32_t preparing=0;
void Window::ReleaseOriginal(){assert(retiring&&task->retired&&window&&resources);assert(mutex.try_lock());mutex.unlock();resources=false;}
void vTaskDelete(void*handle){assert(!handle&&!window&&weak.expired()&&!resources);++deletes;throw Exit{};}
int xTaskCreate(void(*fn)(void*),const char*,unsigned stack,void*ctx,unsigned priority,void*){assert(stack==3072&&priority==3);++creates;if(!create_ok)return 0;entry=fn;context=ctx;return pdPASS;}
'''
fixture+='\n'+method('struct Preparation')+';\n'+method('bool NativeVoiceTaskCleanupComplete()')+'\n'+method('std::shared_ptr<Window> Get()')+'\n'+method('void Reap(void*context)')+'\n'+method('bool EnsureReaper(const std::shared_ptr<Window>&owner)')+'\n'+method('bool StartPreparedNativeVoiceTask()')
fixture+=r'''
int main(){assert(NativeVoiceTaskCleanupComplete());{Preparation borrowed;assert(borrowed&&!NativeVoiceTaskCleanupComplete());}assert(NativeVoiceTaskCleanupComplete());window=std::make_shared<Window>();weak=window;resources=true;assert(!EnsureReaper(window)&&!window->reaping&&window.use_count()==1&&creates==1);create_ok=true;assert(EnsureReaper(window)&&window->reaping&&window.use_count()==2&&creates==2);assert(EnsureReaper(window)&&creates==2);auto api=Get();assert(api&&window.use_count()==3);
 during_delay=[&]{if(!window->creation_finished){window->creation_finished=true;return;}if(!window->task->ready){window->task->ready=true;return;}assert(window->retiring&&resources&&window->task->retired&&window->task->stops>=1&&!Get());api.reset();};
 try{entry(context);}catch(const Exit&){}assert(!window&&weak.expired()&&!api&&!resources&&deletes==1);
 window=std::make_shared<Window>();weak=window;resources=true;window->revoked=false;unsigned checked=0;window->permission=[&]{if(++checked>2)return false;assert(!StartPreparedNativeVoiceTask());return false;};
 assert(!StartPreparedNativeVoiceTask()&&checked==1&&window->started&&window->creation_finished&&window->revoked&&!window->task->starts);during_delay=[&]{assert(window);window->task->ready=true;};try{entry(context);}catch(const Exit&){}assert(!window&&weak.expired()&&!resources&&deletes==2);std::puts("PASS actual native reaper body: API borrows drain before original release and slot cleanup; synthetic task port only");}
'''
with tempfile.TemporaryDirectory(prefix='voice-reaper-') as temp:
 t=Path(temp);cpp=t/'test.cpp';cpp.write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)
