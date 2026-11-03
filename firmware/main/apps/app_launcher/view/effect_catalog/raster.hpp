#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <limits>
constexpr float PI=3.14159265358979323846f;
constexpr uint16_t BLACK=0, WHITE=65535, RED=0xf800,GREEN=0x07e0,BLUE=31,YELLOW=0xffe0,CYAN=0x07ff,MAGENTA=0xf81f,ORANGE=0xfd20;
template<class A,class B> auto min(A a,B b)->decltype(a+b){return a<b?a:b;}
template<class A,class B> auto max(A a,B b)->decltype(a+b){return a>b?a:b;}
template<class A,class B,class C> auto constrain(A a,B b,C c)->decltype(a+b+c){return min(max(a,b),c);}
inline uint32_t& randomState(){static uint32_t state=0x71352;return state;}
inline void resetRandom(uint32_t seed){randomState()=seed?seed:0x71352;}
inline long random(long a,long b){auto& state=randomState();state^=state<<13;state^=state>>17;state^=state<<5;return b>a?a+state%uint32_t(b-a):a;}
inline long random(long b){return random(0,b);}
inline long map(long x,long a,long b,long c,long d){return b==a?c:(x-a)*(d-c)/(b-a)+c;}
class Raster {
 uint16_t* pixels=nullptr; int w=320,h=240,cx=0,cy=0,scale=1;uint16_t ink=WHITE;
 uint32_t remaining_=UINT32_MAX, checked_=0;
 bool exhausted_=false; uint64_t (*clock_)()=nullptr; uint64_t deadline_=0;
 bool consume(uint32_t work=1){
  if(exhausted_||!pixels)return false;
  if(work>remaining_){exhausted_=true;return false;}
  remaining_-=work;checked_+=work;
  if(clock_&&checked_>=256){checked_=0;if(clock_()>=deadline_){exhausted_=true;return false;}}
  return true;
 }
 static int coord(double d){return std::isfinite(d)?int(constrain(d,-2048.0,2048.0)):0;}
 public:
 void beginFrame(uint32_t work,uint64_t (*clock)()=nullptr,uint64_t max_us=12000){remaining_=work;checked_=0;exhausted_=false;clock_=clock;deadline_=clock?clock()+max_us:0;}
 bool exhausted()const{return exhausted_;}
 void bind(uint16_t* p,int width,int height){pixels=p;w=width;h=height;cx=cy=0;scale=1;ink=WHITE;}
 void unbind(){pixels=nullptr;}
 int width(){return w;}int height(){return h;}
 uint16_t color565(int r,int g,int b){return (uint16_t(r&255)>>3)<<11 | (uint16_t(g&255)>>2)<<5 | (uint16_t(b&255)>>3);}
 void drawPixel(double x,double y,uint16_t c){int a=coord(x),b=coord(y);if(consume()&&a>=0&&a<w&&b>=0&&b<h)pixels[b*w+a]=c;}
 void fillScreen(uint16_t c){fillRect(0,0,w,h,c);}
 void fillRect(double x,double y,double width,double height,uint16_t c){int a=coord(x),b=coord(y),ww=coord(width),hh=coord(height);const int left=max(0,a),right=min(w,a+ww);
  if(right<=left)return;
  for(int j=max(0,b);j<min(h,b+hh);j++){if(!consume(uint32_t(right-left)))return;std::fill_n(pixels+j*w+left,right-left,c);}}
 void drawLine(double x,double y,double xx,double yy,uint16_t c){int a=coord(x),b=coord(y),v=coord(xx),z=coord(yy);int dx=std::abs(v-a),sx=a<v?1:-1,dy=-std::abs(z-b),sy=b<z?1:-1,e=dx+dy;for(int n=0;n<8194&&!exhausted_;n++){drawPixel(a,b,c);if(a==v&&b==z)break;int ee=2*e;if(ee>=dy){e+=dy;a+=sx;}if(ee<=dx){e+=dx;b+=sy;}}}
 void drawFastHLine(double x,double y,double ww,uint16_t c){fillRect(x,y,ww,1,c);}void drawFastVLine(double x,double y,double hh,uint16_t c){fillRect(x,y,1,hh,c);}
 void drawRect(double x,double y,double ww,double hh,uint16_t c){drawFastHLine(x,y,ww,c);drawFastHLine(x,y+hh-1,ww,c);drawFastVLine(x,y,hh,c);drawFastVLine(x+ww-1,y,hh,c);}
 // One square root per clipped scanline, rather than two divides per pixel.
 void fillEllipse(double x,double y,double rx,double ry,uint16_t c){
  if(!pixels||exhausted_)return;
  int a=coord(x),b=coord(y),r=std::abs(coord(rx)),s=std::abs(coord(ry));if(!r||!s)return;
  for(int j=max(0,b-s);j<min(h,b+s+1)&&!exhausted_;++j){
   const double dy=(j-b)/double(s);const int span=int(r*std::sqrt(max(0.0,1.0-dy*dy)));
   fillRect(a-span,j,span*2+1,1,c);
  }
 }
 void fillCircle(double x,double y,double r,uint16_t c){fillEllipse(x,y,r,r,c);}
 // Integer midpoint circle: no trigonometry in the pixel loop.
 void drawCircle(double x,double y,double rr,uint16_t c){
  if(!pixels||exhausted_)return;
  int a=coord(x),b=coord(y),r=std::abs(coord(rr));if(r>1024)return;
  int u=r,v=0,error=1-r;
  while(u>=v&&!exhausted_){
   drawPixel(a+u,b+v,c);drawPixel(a+v,b+u,c);drawPixel(a-v,b+u,c);drawPixel(a-u,b+v,c);
   drawPixel(a-u,b-v,c);drawPixel(a-v,b-u,c);drawPixel(a+v,b-u,c);drawPixel(a+u,b-v,c);
   ++v;if(error<0)error+=2*v+1;else{--u;error+=2*(v-u)+1;}
  }
 }
 void drawTriangle(double x,double y,double a,double b,double j,double k,uint16_t c){drawLine(x,y,a,b,c);drawLine(a,b,j,k,c);drawLine(j,k,x,y,c);}
 void fillTriangle(double x,double y,double a,double b,double j,double k,uint16_t c){int x0=coord(x),y0=coord(y),x1=coord(a),y1=coord(b),x2=coord(j),y2=coord(k);auto cross=[](int u,int v,int p,int q,int r,int s){return int64_t(r-u)*(q-v)-int64_t(s-v)*(p-u);};const int left=max(0,min(x0,min(x1,x2))),right=min(w-1,max(x0,max(x1,x2)));if(right<left)return;for(int yy=max(0,min(y0,min(y1,y2)));yy<=min(h-1,max(y0,max(y1,y2)));yy++){if(!consume(uint32_t(right-left+1)))return;for(int xx=left;xx<=right;xx++){auto d=cross(x0,y0,x1,y1,xx,yy),e=cross(x1,y1,x2,y2,xx,yy),f=cross(x2,y2,x0,y0,xx,yy);if((d>=0&&e>=0&&f>=0)||(d<=0&&e<=0&&f<=0))pixels[yy*w+xx]=c;}}}
 void drawRoundRect(double x,double y,double ww,double hh,double,uint16_t c){drawRect(x,y,ww,hh,c);}void fillRoundRect(double x,double y,double ww,double hh,double,uint16_t c){fillRect(x,y,ww,hh,c);}
 void setCursor(int x,int y){cx=x;cy=y;}void setTextSize(int s){scale=constrain(s,1,8);}void setTextColor(uint16_t c){ink=c;}void setTextColor(uint16_t c,uint16_t){ink=c;}
 // Compact original3x5 glyph set; no dependency on Arduino font assets.
 void write(char ch){static const uint16_t glyphs[]={0x7b6f,0x2c97,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};static const uint16_t letters[]={0x2bed,0x6bae,0x3923,0x6b6e,0x79a7,0x79a4,0x396b,0x5bed,0x7497,0x126a,0x5bad,0x4927,0x5fed,0x5ffd,0x2b6a,0x6ba4,0x2b7b,0x6bad,0x388e,0x7492,0x5b6f,0x5b6a,0x5bfd,0x5aad,0x5a92,0x72a7};char upper=ch>='a'&&ch<='z'?ch-32:ch;uint16_t bits=ch>='0'&&ch<='9'?glyphs[ch-'0']:upper>='A'&&upper<='Z'?letters[upper-'A']:0;if(ch!=' ')for(int r=0;r<5;r++)for(int c=0;c<3;c++)if(bits&(1<<(14-r*3-c)))fillRect(cx+c*scale,cy+r*scale,scale,scale,ink);cx+=4*scale;}
 void printf(const char* fmt,...){char text[192];va_list args;va_start(args,fmt);std::vsnprintf(text,sizeof(text),fmt,args);va_end(args);for(char* p=text;*p;p++)write(*p);}
};
