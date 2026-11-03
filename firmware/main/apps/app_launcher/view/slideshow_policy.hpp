#pragma once
#include <array>
#include "screensaver_filter.hpp"
#include <cstdint>
#include <string>
#include <string_view>
namespace view::saver {
constexpr unsigned SlideCount=342;
constexpr uint32_t SlideDurationMs=30000;
enum class Order : uint8_t { Sequential, Random };
struct Selection {
    std::array<uint8_t,(SlideCount+7)/8> bits{};
    Order order=Order::Sequential;
    bool selected(unsigned id) const {return Allowed(id)&&id<SlideCount&&(bits[id/8]&(1u<<(id%8)));}
    void set(unsigned id,bool yes){if(id<SlideCount){if(yes&&!Allowed(id))return;const uint8_t mask=uint8_t(1u<<(id%8));if(yes)bits[id/8]|=mask;else bits[id/8]&=uint8_t(~mask);}}
    unsigned count() const {unsigned n=0;for(unsigned i=0;i<SlideCount;++i)n+=selected(i);return n;}
    unsigned first() const {for(unsigned i=0;i<SlideCount;++i)if(selected(i))return i;return 0;}
    void normalize(){for(auto id:ExcludedSlides)set(id,false);bits.back()&=uint8_t((1u<<(SlideCount%8))-1);if(!count())set(0,true);if(order!=Order::Sequential&&order!=Order::Random)order=Order::Sequential;}
    static Selection defaults(){Selection s;for(unsigned i=0;i<SlideCount;++i)s.set(i,true);return s;}
    std::string encode() const {constexpr char hex[]="0123456789abcdef";std::string out;for(auto byte:bits){out+=hex[byte>>4];out+=hex[byte&15];}return out;}
    std::string payload() const {return std::string(order==Order::Random?"1:1:":"1:0:")+encode();}
    static Selection fromPayload(std::string_view payload){
        if(payload.size()!=4+((SlideCount+7)/8)*2||payload[0]!='1'||payload[1]!=':'||payload[3]!=':'||(payload[2]!='0'&&payload[2]!='1'))return defaults();
        return decode(payload.substr(4),payload[2]-'0');
    }
    static Selection decode(std::string_view encoded,int mode){
        Selection s=defaults();
        if(encoded.size()==s.bits.size()*2){
            s.bits.fill(0);
            auto digit=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
            for(unsigned i=0;i<s.bits.size();++i){int a=digit(encoded[i*2]),b=digit(encoded[i*2+1]);if(a<0||b<0)return defaults();s.bits[i]=uint8_t(a*16+b);}
        }
        s.order=mode==1?Order::Random:Order::Sequential;s.normalize();return s;
    }
};
class Slideshow {
    Selection selection_{};unsigned current_=0;uint32_t since_=0;
public:
    unsigned start(Selection selection,uint32_t now){selection.normalize();selection_=selection;current_=selection.first();since_=now;return current_;}
    template<class Random> bool advance(uint32_t now,Random random){
        if(uint32_t(now-since_)<SlideDurationMs)return false;
        since_=now;const unsigned count=selection_.count();if(count<2)return false;
        if(selection_.order==Order::Sequential){for(unsigned n=1;n<=SlideCount;++n){unsigned id=(current_+n)%SlideCount;if(selection_.selected(id)){current_=id;break;}}}
        else {unsigned target=random()%(count-1);for(unsigned id=0;id<SlideCount;++id)if(selection_.selected(id)&&id!=current_){if(target==0){current_=id;break;}--target;}}
        return true;
    }
    unsigned current() const{return current_;}
};
// Stock launcher owns idle entry. Foreign screens/navigation/audio preempt it.
inline bool MayEnterIdle(uint32_t idle,bool launcher_active,bool navigation_pending,bool media_owned){
    return idle>=30000&&launcher_active&&!navigation_pending&&!media_owned;
}
}
