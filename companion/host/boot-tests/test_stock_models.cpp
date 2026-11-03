#include "toolbox_models.hpp"
#include <cassert>
#include <iostream>
int main(){
 stock_tools::Game2048 g;g.cells={2,2,2,2};assert(g.Move(0));assert(g.cells[0]==4 && g.cells[1]==4 && g.cells[2]==0 && g.score==8);assert(g.Move(0));assert(g.cells[0]==8 && g.score==16);assert(!g.Move(0));assert(g.AddTile(0));
 g.cells={2,4,8,16,4,8,16,2,8,16,2,4,16,2,4,8};assert(!g.CanMove());assert(!g.AddTile(1));assert(!g.Move(-1));
 g.cells.fill(0);g.cells[0]=32768;g.cells[1]=32768;assert(!g.Move(0));assert(g.cells[0]==32768 && g.cells[1]==32768);
 g.cells.fill(0);g.cells[0]=2;g.cells[1]=2;g.score=UINT32_MAX-2;assert(g.Move(0) && g.score==UINT32_MAX);
 auto start=uint64_t(UINT32_MAX)-5;assert(!stock_tools::WindowExpired(start+9999,start,10000));assert(stock_tools::WindowExpired(start+10000,start,10000));assert(stock_tools::WindowExpired(1,UINT64_MAX-5,10000));
 g.Reset();assert(g.cells[0]==2 && g.cells[15]==2 && g.score==0);assert(g.Move(2));
 std::array<uint8_t,180> tag{};std::array<uint8_t,7> uid={4,1,2,3,4,5,6};assert(stock_tools::UrlTag("https://example.com",uid,tag));assert(tag[3]==(0x88^4^1^2) && tag[8]==(3^4^5^6));assert(tag[16]==3 && tag[18]==0xd1 && tag[21]==0x55 && tag[22]==4 && tag[34]==0xfe);
 assert(!stock_tools::UrlTag("javascript:x",uid,tag));assert(!stock_tools::UrlTag("https://",uid,tag));assert(!stock_tools::UrlTag("https://user@host",uid,tag));assert(!stock_tools::UrlTag("https://example.com/\n",uid,tag));assert(!stock_tools::UrlTag("https://"+std::string(89,'a'),uid,tag));
 std::vector<uint32_t> pulses={9000,4500};uint32_t expected=0x20df10ef;for(int i=0;i<32;++i){pulses.push_back(560);pulses.push_back(expected&(uint32_t(1)<<i)?1690:560);}pulses.push_back(560);uint32_t value=0;assert(stock_tools::NecDecode(pulses,value) && value==expected);pulses[10]=200;assert(!stock_tools::NecDecode(pulses,value));pulses.resize(66);assert(!stock_tools::NecDecode(pulses,value));
 std::cout<<"PASS 2048 single-merge/directions/no-op/full/overflow; NFC Type2 URI/bounds; NEC decode/timing rejection\n";
}
