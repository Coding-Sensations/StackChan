#pragma once
#include "local_tools.hpp"
#include <array>
namespace stackchan_tools {
struct DiceExpression {
 unsigned count=1,sides=20;int modifier=0;
 bool Valid()const{return count>=1&&count<=20&&sides>=2&&sides<=1000&&modifier>=-100&&modifier<=100;}
};
struct DiceResult { std::array<uint32_t,20> faces{};unsigned count=0;int total=0; };
// Eight rejection attempts per die; publish only a complete result.
// Each attempt gets a fresh hardware word from the caller.
template<class Random> std::optional<DiceResult> RollExpression(const DiceExpression& expression,Random&& random) {
 if(!expression.Valid())return {};
 DiceResult result;result.count=expression.count;result.total=expression.modifier;
 for(unsigned i=0;i<expression.count;++i){
  std::optional<uint32_t> face;
  for(unsigned attempt=0;attempt<8&&!face;++attempt)face=Dice(expression.sides,random());
  if(!face)return {};
  result.faces[i]=*face;result.total+=int(*face);
 }
 return result;
}
}
