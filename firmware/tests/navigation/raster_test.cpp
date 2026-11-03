#include "apps/app_launcher/view/effect_catalog/raster.hpp"
#include <array>
#include <cassert>
#include <limits>
static uint64_t Clock(){static uint64_t now=0;now+=13000;return now;}
int main(){
 std::array<uint16_t,320*240+2> buffer{};buffer.front()=0x1234;buffer.back()=0xabcd;
 Raster raster;raster.bind(buffer.data()+1,320,240);
 raster.beginFrame(100);
 raster.fillScreen(0xffff);
 assert(raster.exhausted());
 raster.beginFrame(1000000);
 raster.fillCircle(160,120,40,0xffff);
 assert(!raster.exhausted());assert(buffer[1+120*320+160]==0xffff);
 raster.drawCircle(-5000,1,5000,1);
 raster.fillTriangle(-5000,-5000,5000,5000,std::numeric_limits<double>::infinity(),5,2);
 assert(buffer.front()==0x1234&&buffer.back()==0xabcd);
 raster.beginFrame(1000000,Clock);raster.fillScreen(3);assert(raster.exhausted());
 raster.unbind();raster.fillScreen(1);raster.fillRect(0,0,320,240,1);
}
