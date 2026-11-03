#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
namespace audio_view {
enum class Tool {Home,PitchPipe,Fork,Guitar,Piano,Key,Spectrogram,Meter,Metro};
struct Rect{int x,y,w,h;};
inline Rect Card(unsigned i){return {8+int(i%2)*156,46+int(i/2)*42,148,38};}
inline const char* Name(Tool tool){const char* names[]={"Audio tools","Pitch pipe","Tuning fork","Guitar tuner","Piano tuner","Key estimate","Spectrogram","Level meter","Metronome"};return names[unsigned(tool)];}
inline constexpr std::array<unsigned,6> GuitarNotes{40,45,50,55,59,64};
inline double Frequency(int midi){return 440.0*std::pow(2.,(midi-69)/12.);}
inline std::string Note(int midi){if(midi<0||midi>127)return "--";const char* notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return std::string(notes[midi%12])+std::to_string(midi/12-1);}
inline double Cents(double hz,int target){return std::isfinite(hz)&&hz>0?1200*std::log2(hz/Frequency(target)):0;}
inline unsigned NearestString(double hz){unsigned result=0;double best=1e9;for(unsigned i=0;i<GuitarNotes.size();++i){double d=std::abs(Cents(hz,GuitarNotes[i]));if(d<best){best=d;result=i;}}return result;}
inline unsigned Bar(double dbfs){return std::isfinite(dbfs)?unsigned(std::clamp((dbfs+60)/60*100,0.,100.)):0;}
inline uint16_t Heat(uint8_t intensity){unsigned r=std::min(255u,unsigned(intensity)*2),g=intensity>64?std::min(255u,(unsigned(intensity)-64)*2):0,b=intensity<128?unsigned(intensity):255-unsigned(intensity);return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
inline std::string Signature(int tonic,bool minor){if(tonic<0||tonic>11)return "Insufficient notes";const char* names[]={"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};const char* minors[]={"C","C#","D","D#","E","F","F#","G","G#","A","Bb","B"};const int counts[]={0,-5,2,-3,4,-1,6,1,-4,3,-2,5};int count=counts[(tonic+(minor?3:0))%12];std::string key=std::string(minor?minors[tonic]:names[tonic])+(minor?" minor":" major");return key+" / "+(count==0?std::string("no accidentals"):std::to_string(std::abs(count))+(count>0?" sharps":" flats"));}
}
