#include "muse_codec_fixture.hpp"
#define main original_bridge_fixture_main
#include "voice_bridge_test.cpp"
#undef main
#include <companion/muse_voice_review_permit.hpp>
#include <companion/muse_voice_core_codec.hpp>
#include "voice_pcm_fixture.hpp"
#include "voice_native_authority_fixture.hpp"
using Bound=muse_native_review::ReviewBoundPermit;
using Reviewed=muse_native_review::ReviewedDirectionOwner;
int main(){
 for(unsigned mask=0;mask<4;++mask){
  voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,mask&1,mask&2);Matching native;native.expected=bridge.Review().Receipt();Bound bound(p,bridge,clock,native);
  assert(bound.BindReview()==bool(mask));assert(!bound.BindReview());
  if(mask){assert(bound.VoiceContext(7,4000)&&bound.Provider(7,4000));assert(bound.Microphone(7,4000)==bool(mask&1));assert(bound.Output(7,4000)==bool(mask&2));const auto before=native.calls;assert(!bound.VoiceContext(8,4000)&&native.calls==before);assert(bound.VoiceContext(7,4000));}
  assert(!bridge.Review().NativePermit()&&CurrentAudioOwner()==AudioOwner::None);
 }
 {
  Reset();auto core=Make();companion::muse::CoreVoiceCodec<CoreS3AudioCodec> codec(core);voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Reviewed owner(p,bridge,codec,native,clock);
  assert(owner.Prepare()&&owner.Generation()==7&&owner.Start(4000));assert(CurrentAudioOwner()==AudioOwner::Muse&&!opens&&!reads&&!writes);
  assert(owner.Stop(4000)&&CurrentAudioOwner()==AudioOwner::None);assert(!owner.Prepare());
 }
 {
  Reset();auto core=Make();companion::muse::CoreVoiceCodec<CoreS3AudioCodec> codec(core);voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);BooleanOnly native;Reviewed owner(p,bridge,codec,native,clock);
  assert(!owner.Prepare()&&CurrentAudioOwner()==AudioOwner::None&&!native.calls&&!opens&&!reads&&!writes);
 }
 {
  voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Bound bound(p,bridge,clock,native);
  clock.during=[&]{bound.Revoke();};assert(!bound.BindReview()&&!bound.VoiceContext(7,4000)&&!bound.BindReview());
 }
 for(unsigned field=0;field<9;++field){
  voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Bound bound(p,bridge,clock,native);assert(bound.BindReview());
  auto&t=native.expected.target;switch(field){case 0:t.device[0]='X';break;case 1:t.call[0]='X';break;case 2:t.session[0]='X';break;case 3:t.thread[0]='X';break;case 4:++t.generation;break;case 5:--t.expires.mono;break;case 6:--t.expires.wall;break;case 7:native.expected.microphone=false;break;case 8:native.expected.speaker=false;break;}
  assert(!bound.VoiceContext(7,4000)&&!native.calls&&!bound.BindReview());
 }
 for(unsigned seam=0;seam<3;++seam)for(unsigned failure=0;failure<8;++failure){
  voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Bound bound(p,bridge,clock,native);assert(bound.BindReview());
  const auto fail=[&]{switch(failure){case 0:bound.Revoke();break;case 1:bridge.Revoke();break;case 2:custody.current=false;break;case 3:clock.time.mono_us=10000000;break;case 4:clock.time.wall_ms=1760000010000LL;break;case 5:--clock.time.mono_us;break;case 6:--clock.time.wall_ms;break;case 7:bridge.Review().Cancel();assert(bridge.Review().Open(bridge.Now()));Complete(bridge,true,true);break;}};
  if(seam<2){native.trigger=seam+1;native.during=fail;}else native.operation=fail;
  assert(!bound.VoiceContext(7,4000)&&!bound.BindReview()&&CurrentAudioOwner()==AudioOwner::None);
 }
 {
  voice_prep::Protocol p,other;Live(p);Live(other);TestClock clock,other_clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Bound wrong_protocol(other,bridge,clock,native),wrong_clock(p,bridge,other_clock,native);
  assert(!wrong_protocol.BindReview()&&!wrong_clock.BindReview()&&!native.calls&&!native.matches&&CurrentAudioOwner()==AudioOwner::None);
 }
 {
  voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Bound bound(p,bridge,clock,native);assert(bound.BindReview());
  native.during=[&]{assert(!bound.BindReview()&&!bound.VoiceContext(7,4000));};assert(bound.VoiceContext(7,4000));clock.time.mono_us=5000;assert(bound.Hold(7,5,4000)&&native.seen==5000);assert(!bound.Hold(7,0,4000));assert(bound.VoiceContext(7,4000));bound.Revoke();assert(!bound.VoiceContext(7,4000)&&!bound.BindReview());
 }
 {
  Reset();auto core=Make();companion::muse::CoreVoiceCodec<CoreS3AudioCodec> codec(core);voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);voice_codec_prep::NativeUnavailablePermit native;Reviewed owner(p,bridge,codec,native,clock);
  assert(!owner.Prepare()&&CurrentAudioOwner()==AudioOwner::None&&!opens&&!reads&&!writes&&owner.Stop(4000));
 }
 {
  Reset();auto core=Make();companion::muse::CoreVoiceCodec<CoreS3AudioCodec> codec(core);voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Reviewed owner(p,bridge,codec,native,clock);assert(owner.Prepare()&&owner.Start(4000));
  disable_result=-1;assert(!owner.Stop(4000)&&CurrentAudioOwner()==AudioOwner::Muse&&!owner.Start(4000));disable_result=0;assert(owner.Stop(4000)&&CurrentAudioOwner()==AudioOwner::None);
 }
 {
  Reset();auto core=Make();companion::muse::CoreVoiceCodec<CoreS3AudioCodec> codec(core);voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Reviewed owner(p,bridge,codec,native,clock);
  native.during=[&]{assert(!owner.Prepare());owner.Stop(4000);};assert(!owner.Prepare()&&owner.Stop(4000)&&CurrentAudioOwner()==AudioOwner::None&&!opens&&!reads&&!writes);
 }
 {
  Reset();auto core=Make();companion::muse::CoreVoiceCodec<CoreS3AudioCodec> codec(core);voice_prep::Protocol p;Live(p);TestClock clock;TestCustody custody;Bridge bridge(p,clock,custody);assert(bridge.Open());Complete(bridge,true,true);Matching native;native.expected=bridge.Review().Receipt();Reviewed owner(p,bridge,codec,native,clock);assert(owner.Prepare()&&owner.Start(4000));native.current=false;assert(!owner.Poll(4000)&&CurrentAudioOwner()==AudioOwner::None&&owner.Stop(4000));
 }
 std::puts("PASS actual returned-review/native-match/retained Owner composition; synthetic native match is not genuine authority");
}
