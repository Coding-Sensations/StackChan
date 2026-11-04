#include "hal/board/aw9523_p0.hpp"
#include <cassert>
#include <vector>
// Fake expander: records every transfer in order so blind or reordered writes are visible.
struct Fake{
 uint8_t out=0b10100111,config=0b00011000;int read_fail_at=-1,write_status=0,read_status=0,reads=0;bool corrupt=false,drop=false;
 std::vector<uint8_t> writes;
 int Read(uint8_t reg,uint8_t&v){if(reads++==read_fail_at)return read_status;v=reg==aw9523::kOutP0?out:config;return 0;}
 int Write(uint8_t reg,uint8_t v){assert(reg==aw9523::kOutP0);writes.push_back(v);if(write_status)return write_status;if(drop)return 0;out=corrupt?uint8_t(v^0x80):v;return 0;}
};
int main(){
 using namespace aw9523;
 {Fake f;assert(SetP0Bit(f,2,false)==0&&f.out==0b10100011&&f.writes.size()==1);   // only P0_2 changes
  assert(SetP0Bit(f,2,true)==0&&f.out==0b10100111&&f.writes.size()==2);}
 {Fake f;f.out=0xff;assert(SetP0Bit(f,2,false)==0&&f.out==0xfb);f.out=0x00;assert(SetP0Bit(f,2,true)==0&&f.out==0x04);} // any other bits survive
 {Fake f;assert(SetP0Bit(f,3,false)==kNotOutput&&f.writes.empty());}               // bit 3 is an input: never written
 {Fake f;assert(SetP0Bit(f,8,true)==kNotOutput&&f.writes.empty());}
 for(int at=0;at<3;++at){Fake f;f.read_fail_at=at;f.read_status=-7;assert(SetP0Bit(f,2,false)==-7);assert(f.writes.empty()||at==2);} // read errors surface (index 2 = readback bus error, a failure not success); no write before config/out are known
 {Fake f;f.write_status=-9;assert(SetP0Bit(f,2,false)==-9&&f.out==0b10100111);}    // write error: latch untouched, reported
 {Fake f;f.corrupt=true;assert(SetP0Bit(f,2,false)==kReadbackMismatch);}           // readback differs from request
 {Fake f;f.drop=true;assert(SetP0Bit(f,2,false)==kReadbackMismatch);}              // write silently lost
 return 0;
}
