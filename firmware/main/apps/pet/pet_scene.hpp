#pragma once
#include <apps/local_tools/local_tools.hpp>
#include <cstdint>
namespace pet_game {
enum class Mood { Happy, Content, Hungry, Tired, Lonely };
enum class Care { None, Feed, Play, Rest };
struct Frame {int x=116,y=26;bool blink=false,bowl=false,spark=false,sleep=false;Mood mood=Mood::Content;unsigned phase=0;};
inline Mood Needs(const stackchan_tools::Pet& p){if(p.hunger>=75)return Mood::Hungry;if(p.energy<30)return Mood::Tired;if(p.happiness<35)return Mood::Lonely;return p.happiness>=70?Mood::Happy:Mood::Content;}
inline const char* Cue(Mood m){switch(m){case Mood::Hungry:return "A snack, please!";case Mood::Tired:return "A nap would help";case Mood::Lonely:return "Play with me!";case Mood::Happy:return "Tap your pal to play";case Mood::Content:return "A little care goes a long way";}return "";}
inline Frame Animate(const stackchan_tools::Pet& p,Care care,uint64_t started,uint64_t now){
 Frame f;f.mood=Needs(p);f.phase=unsigned((now/160)%4);f.y+=f.phase==1?-2:f.phase==3?2:0;f.blink=(now/160)%24==0;const bool active=now>=started&&now-started<1600;
 if(active&&care==Care::Feed){f.bowl=true;f.blink=f.phase==1;}
 if(active&&care==Care::Play){f.spark=true;f.x+=f.phase==0?-6:f.phase==2?6:0;f.y-=f.phase==1?5:0;}
 f.sleep=(active&&care==Care::Rest)||f.mood==Mood::Tired;if(f.sleep)f.blink=true;return f;
}
// Original 12x12 mint creature silhouette, designed for this project. No imported sprites.
inline constexpr const char* Sprite[]={"..AA....AA..",".AAAA..AAAA.",".AAAAAAAAAA.","AAAAAAAAAAAA","AAAAAAAAAAAA","AAAAAAAAAAAA","AAAAAAAAAAAA","AAAAAAAAAAAA",".AAAAAAAAAA.","..AAAAAAAA..","...AA..AA...","...AA..AA..."};
}
