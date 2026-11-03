#include "muse_codec_fixture.hpp"
#include "muse_amplifier_fixture.hpp"
#include <companion/muse_local_diagnostic_ui.hpp>
#include <cstdlib>
#include <type_traits>
#include <stdexcept>
using namespace companion::muse::local_diag;
static_assert(!std::is_default_constructible_v<RunGrant> && !std::is_copy_constructible_v<RunGrant>);
struct Port:Driver {
 mutable lv_indev_t pointer;lv_indev_t*published=&pointer;int core_calls=0,bus_calls=0;bool bad_bus=false,throw_core=false,throw_bus=false,throw_input=false,throw_tick=false;
 lv_indev_t*Input()const override{if(throw_input)throw std::runtime_error("inert input fault");return published;}uint32_t Tick()const override{if(throw_tick)throw std::runtime_error("inert tick fault");return native_tick;}
 Codec&Core()override{++core_calls;if(throw_core)throw std::runtime_error("inert core fault");return Codec::MuseColdOutputOnly();}
 void*Bus()override{++bus_calls;if(throw_bus)throw std::runtime_error("inert bus fault");return bad_bus?nullptr:reinterpret_cast<void*>(9);}
};
static void Down(lv_obj_t*b,lv_indev_t&p){active_input=event_input=&p;p.state=LV_INDEV_STATE_PRESSED;p.active=b;Press(b);active_input=event_input=nullptr;}
static void Up(lv_obj_t*b,lv_indev_t&p){active_input=event_input=&p;p.state=LV_INDEV_STATE_RELEASED;p.active=b;ReleaseClick(b);active_input=event_input=nullptr;}
static void Touch(Port&p,const char*label){auto*b=Button(label);assert(b);Down(b,p.pointer);Up(b,p.pointer);}
static int Calls(){return cold_calls+amp_calls+info_calls+preload_calls+opens+closes+disables+reads+writes;}
int main(int argc,char**argv){assert(argc==2);const int test=std::atoi(argv[1]);Reset();auto normal=Make();assert(!normal.ColdOutputOnlyInstance());cold_amplifier=&amplifier;Port port;Ui ui(port);assert(ui.Open(&screen)&&Calls()==0&&CurrentAudioOwner()==AudioOwner::None);
 // Program-lifetime owner mirrors native retention. Fixture process exit is
 // not device recovery or a fabricated close/join receipt.
 alignas(Owner) static unsigned char storage[sizeof(Owner)];auto&owner=*new(storage)Owner(port);
 if(test==0){Click(Button("Arm diagnostic"));assert(Button("Arm diagnostic")&&!ui.TakeRun()&&!Calls());ui.Close();return 0;}
 if(test==1){lv_indev_t foreign;auto*b=Button("Arm diagnostic");Down(b,foreign);Up(b,foreign);assert(Button("Arm diagnostic")&&!ui.TakeRun());ui.Close();return 0;}
 if(test==2){auto*b=Button("Arm diagnostic");Up(b,port.pointer);assert(Button("Arm diagnostic")&&!ui.TakeRun());ui.Close();return 0;}
 if(test==3){auto*b=Button("Arm diagnostic");Down(b,port.pointer);native_tick+=10001;Up(b,port.pointer);assert(Button("Arm diagnostic"));ui.Close();return 0;}
 if(test==4){auto*b=Button("Arm diagnostic");Down(b,port.pointer);lv_event_t e{b,b->callback_user,LV_EVENT_PRESS_LOST};b->callback(&e);Up(b,port.pointer);assert(Button("Arm diagnostic"));ui.Close();return 0;}
 Touch(port,"Arm diagnostic");assert(Button("Run once")&&Button("Cancel")&&!Calls());
 if(test==5){Click(Button("Run once"));assert(!ui.TakeRun()&&!Calls());ui.Close();return 0;}
 if(test==6){Touch(port,"Cancel");assert(Button("Arm diagnostic")&&!ui.TakeRun()&&!Calls());ui.Close();return 0;}
 if(test==7){native_tick+=10001;Touch(port,"Run once");assert(!ui.TakeRun()&&!Calls());ui.Close();return 0;}
 if(test==8){lv_indev_t foreign;auto*b=Button("Run once");Down(b,foreign);Up(b,foreign);assert(!ui.TakeRun()&&!Calls());ui.Close();return 0;}
 Touch(port,"Run once");auto grant=ui.TakeRun();assert(grant&&!ui.TakeRun()&&!Calls()&&!Button("Run once"));
 if(test==9)ui.Close();if(test==10)native_tick+=10001;lv_indev_t replacement;if(test==11)port.published=&replacement;
 if(test==27)port.throw_input=true;if(test==28)port.throw_tick=true;
 if(test==29)amp_hook=[&](int n){if(n==8)throw std::runtime_error("inert driver fault");};
 if(test==12){auto stock=Take(AudioOwner::Stock,AudioPurpose::StockRuntime);assert(owner.Run(std::move(*grant))==Owner::State::Busy&&CurrentAudioOwner()==AudioOwner::Stock&&!Calls()&&!port.core_calls);assert(ReleaseAudio(stock,true));ui.Close();return 0;}
 if(test>=13&&test<=17)cold_fail=test-12;
 if(test==18)port.bad_bus=true;
 if(test==19)port.throw_core=true;if(test==20)port.throw_bus=true;
 if(test==21)partial_preload=1;
 if(test==22)fail_at=8; // after initialization quiet; retirement first register
 if(test==23)info_hook=[&]{auto&core=Codec::MuseColdOutputOnly();auto original=core.direct_common_lease_;assert(ReleaseAudio(original,true));auto stock=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);(void)stock;};
 if(test==24)cold_hook=[&]{auto&core=Codec::MuseColdOutputOnly();auto original=core.direct_common_lease_;assert(ReleaseAudio(original,true));auto stock=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);(void)stock;};
 if(test==25)cold_hook=[&]{ui.Close();}; // committed cleanup still completes
 if(test==26)cold_hook=[&]{assert(owner.Status()==Owner::State::Running);char text[1024];assert(!owner.Format(text,sizeof text));assert(owner.Run(std::move(*grant))==Owner::State::Running);};
 if(test==30){port.throw_input=true;assert(owner.Run(std::move(*grant))==Owner::State::Rejected&&!Calls());port.throw_input=false;alignas(Owner)static unsigned char other_storage[sizeof(Owner)];auto&other=*new(other_storage)Owner(port);assert(other.Run(std::move(*grant))==Owner::State::Rejected&&!Calls());ui.Close();return 0;}
 const auto result=owner.Run(std::move(*grant));
 if((test>=9&&test<=11)||test==27||test==28){assert(result==Owner::State::Rejected&&!Calls()&&!port.core_calls&&CurrentAudioOwner()==AudioOwner::None);}
 else if((test>=13&&test<=22)||test==29){assert(result==Owner::State::Retained&&CurrentAudioOwner()==AudioOwner::Muse);const auto calls=Calls();assert(owner.Run(std::move(*grant))==Owner::State::Retained&&Calls()==calls);}
 else if(test==23||test==24){assert(result==Owner::State::CustodyLost&&CurrentAudioOwner()==AudioOwner::Stock);}
 else{assert(result==Owner::State::Complete&&CurrentAudioOwner()==AudioOwner::None&&port.core_calls==1&&port.bus_calls==1&&input_disables==0&&output_disables==0&&opens==0&&closes==0&&reads==0&&writes==0);}
 char text[1024];assert(owner.Format(text,sizeof text));assert(std::string(text).find("physical-flush=unqualified")!=std::string::npos);for(auto s:{"token","password","http","thread","session","0x","/Users"})assert(std::string(text).find(s)==std::string::npos);
 for(size_t n=1;n<30;++n){char small[32];assert(!owner.Format(small,n)&&small[0]==0);}const auto calls=Calls();assert(owner.Run(std::move(*grant))==result&&Calls()==calls);ui.Report(text);ui.Close();assert(!LiveObjects());
 std::printf("PASS local diagnostic case%d; inert ports only\n",test);
}
