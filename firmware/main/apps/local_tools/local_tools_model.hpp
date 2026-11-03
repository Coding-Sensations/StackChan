#pragma once
#include "local_tools.hpp"
#include "dice_expression.hpp"
namespace local_tools_ui {
enum class Screen { Home, Dice, Pet, Wifi, Audio };
class Model {
public:
 Screen screen=Screen::Home;
 stackchan_tools::Pet pet;
 unsigned sides=20,roll=0;
 unsigned dice_count=1;int dice_modifier=0;bool dice_rolled=false,dice_failed=false;
 stackchan_tools::DiceResult dice_result;
 bool connected=false; int rssi=0;
 uint64_t last_ms=0; bool clock_started=false,clock_error=false;
 void Tick(uint64_t now) {
  if(clock_started&&(now<last_ms||!pet.Advance(now-last_ms))){clock_error=true;return;}
  clock_error=false;
  clock_started=true;last_ms=now;
 }
 bool RestorePet(stackchan_tools::Pet restored,uint64_t now){
  if(clock_started&&(now<last_ms||!restored.Advance(now-last_ms))){clock_error=true;return false;}
  if(!restored.Advance(0)){clock_error=true;return false;}
  pet=restored;clock_started=true;clock_error=false;last_ms=now;return true;
 }
 void Select(Screen next){screen=next;}
 void Back(){screen=Screen::Home;}
 bool Roll(uint32_t random){auto result=stackchan_tools::Dice(sides,random);if(!result)return false;roll=*result;return true;}
 template<class Random> bool RollDice(Random&& random){
  dice_rolled=false;dice_failed=false;roll=0;
  auto result=stackchan_tools::RollExpression({dice_count,sides,dice_modifier},random);
  if(!result){dice_failed=true;return false;}
  dice_result=*result;dice_rolled=true;return true;
 }
 void Count(int delta){dice_count=unsigned(std::clamp<int64_t>(int64_t(dice_count)+delta,1,20));dice_rolled=dice_failed=false;}
 void Modifier(int delta){dice_modifier=int(std::clamp<int64_t>(int64_t(dice_modifier)+delta,-100,100));dice_rolled=dice_failed=false;}
 void Die(unsigned next){if(next>=2&&next<=1000){sides=next;roll=0;dice_rolled=dice_failed=false;}}
 void Wifi(bool present,int signal){connected=present;rssi=signal;}
 int Bars()const{return stackchan_tools::WifiBars(rssi,connected);}
};
}
