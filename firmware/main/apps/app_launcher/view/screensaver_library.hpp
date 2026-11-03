#pragma once
#include "slideshow_policy.hpp"
namespace view::saver {
// Built-ins live in application code. Visibility never deletes assets or changes
// the saved playlist; the runtime playlist is their intersection plus DVD fallback.
struct Library {
    Selection selection=Selection::defaults();
    Selection hidden{};
    bool visible(unsigned id) const {return Allowed(id)&&!hidden.selected(id);}
    bool inView(unsigned id,bool hiddenView) const {return Allowed(id)&&(hiddenView?hidden.selected(id):visible(id));}
    void setHidden(unsigned id,bool value){if(id)hidden.set(id,value);}
    unsigned adjacent(unsigned id,bool forward,bool hiddenView) const {
        for(unsigned n=0;n<SlideCount;++n){id=(id+(forward?1:SlideCount-1))%SlideCount;if(inView(id,hiddenView))return id;}
        return 0;
    }
    Selection effective() const {
        auto result=selection;
        for(unsigned id=1;id<SlideCount;++id)if(hidden.selected(id))result.set(id,false);
        result.normalize();return result;
    }
    std::string payload() const {
        auto prefix=selection.payload();prefix[0]='2';return prefix+":"+hidden.encode();
    }
    static Library fromPayload(std::string_view payload){
        Library result;
        constexpr size_t legacyLength=4+((SlideCount+7)/8)*2;
        if(payload.size()>=legacyLength&&payload[0]=='2'){
            std::string original(payload.substr(0,legacyLength));original[0]='1';
            result.selection=Selection::fromPayload(original);
            auto suffix=payload.substr(legacyLength);
            if(suffix.size()==1+result.hidden.bits.size()*2&&suffix[0]==':'){
                suffix.remove_prefix(1);
                for(char c:suffix)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return result;
                result.hidden=Selection::decode(suffix,0);result.hidden.set(0,false);
            }
        }else result.selection=Selection::fromPayload(payload);
        return result;
    }
};
}
