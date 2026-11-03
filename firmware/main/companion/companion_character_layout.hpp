#pragma once
#include <string>
namespace companion::desk {
struct CharacterRect {int x,y,w,h;};
struct CharacterLayout {
    bool supported;
    CharacterRect face,connection,privacy,status,pair,connect,listen;
};
// The production shell has a 320x240 logical minimum. Larger parent surfaces
// retain the same touch controls and expand the passive character viewport.
constexpr CharacterLayout CharacterScreen(int width,int height){
    return {width>=320&&height>=240,
        {0,0,width,height},{8,height-24,width-152,20},
        {width-136,height-24,128,20},{8,48,304,78},{8,130,304,40},
        {8,174,148,40},{164,174,148,40}};
}
inline std::string CharacterConnectionStatus(const char* full){
    std::string text=full&&*full?full:"Status unavailable";
    const std::string title="Mira companion\n";
    if(text.compare(0,title.size(),title)==0)text.erase(0,title.size());
    const auto end=text.find('\n');if(end!=std::string::npos)text.resize(end);
    return text.empty()?"Status unavailable":text;
}
}
