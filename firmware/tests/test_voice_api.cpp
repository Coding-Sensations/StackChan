#include "voice/api.hpp"
#include <cassert>
#include <cstddef>
#include <cstring>
int main(){
 const auto& api=*stackchan_muse_voice_protocol_api();
 assert(api.version==2&&api.bytes==sizeof(voice_prep::Protocol)&&api.alignment==alignof(voice_prep::Protocol));
 alignas(voice_prep::Protocol) unsigned char memory[sizeof(voice_prep::Protocol)+1];
 assert(!api.construct(nullptr,sizeof memory));
 assert(!api.construct(memory,sizeof(voice_prep::Protocol)-1));
 assert(!api.construct(memory+1,sizeof(voice_prep::Protocol)));
 auto* p=api.construct(memory,sizeof memory);assert(p);
 assert((p->*api.status)()==voice_prep::State::Idle);
 voice_prep::TargetSnapshot target{};std::memset(&target,0x55,sizeof target);
 assert(!(p->*api.returned_target)(1,1,target));
 for(auto c:reinterpret_cast<const unsigned char(&)[sizeof(target)]>(target))assert(c==0);
 assert(!(p->*api.begin)({0,"synthetic-call","synthetic-device",1,3,10000},0,1704067200000LL));
 assert((p->*api.begin)({1,"synthetic-call","synthetic-device",1,3,10000},0,1704067200000LL));
 assert((p->*api.pending)().kind==voice_prep::Kind::OpenMedia);
 assert((p->*api.cancel)(1,1));
 assert((p->*api.status)()==voice_prep::State::AwaitTransportClose);
 assert((p->*api.transport_closed)(1,true));
 alignas(muse_native_review::Bridge) unsigned char review_memory[sizeof(muse_native_review::Bridge)+1];
 assert(api.review_bytes==sizeof(muse_native_review::Bridge)&&api.review_alignment==alignof(muse_native_review::Bridge));
 assert(!api.construct_review(nullptr,sizeof review_memory,p,nullptr,nullptr));
 assert(!api.construct_review(review_memory,sizeof(muse_native_review::Bridge)-1,p,nullptr,nullptr));
 assert(!api.construct_review(review_memory+1,sizeof(muse_native_review::Bridge),p,nullptr,nullptr));
 assert(!api.construct_review(review_memory,sizeof review_memory,nullptr,nullptr,nullptr));
 auto* review=api.construct_review(review_memory,sizeof review_memory,p,nullptr,nullptr);assert(review);
 assert(!review->Open()&&!review->Review().Receipt().valid&&!review->Review().NativePermit());
 api.destroy_review(review);for(size_t i=0;i<sizeof(muse_native_review::Bridge);++i)assert(review_memory[i]==0);
 api.destroy_review(nullptr);
 api.destroy(p);for(size_t i=0;i<sizeof(voice_prep::Protocol);++i)assert(memory[i]==0);
 api.destroy(nullptr);
}
