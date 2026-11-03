#pragma once
#include "instrument_engine.hpp"
#include <array>
namespace instrument_ui::synth_view {
struct Rect {int x,y,w,h;};
inline constexpr Rect Preview{8,54,190,44}, Wave{8,108,92,32}, Lower{108,108,32,32}, Upper{172,108,32,32}, Sustain{208,108,104,32}, LevelDown{208,72,48,30}, LevelUp{264,72,48,30};
inline constexpr std::array<unsigned,8> WhiteNotes{0,2,4,5,7,9,11,12};
inline constexpr std::array<unsigned,5> BlackNotes{1,3,6,8,10};
inline constexpr std::array<unsigned,5> BlackAfter{0,1,3,4,5};
inline constexpr Rect White(unsigned i){return {8+int(i)*38,148,38,68};}
inline constexpr Rect Black(unsigned i){return {8+int(BlackAfter[i]+1)*38-12,148,24,40};}
inline unsigned Octave(unsigned base,int delta){return unsigned(std::clamp(int(base)+12*delta,24,96));}
inline uint16_t Level(uint16_t gain,int delta){return uint16_t(std::clamp(int(gain)+512*delta,0,8192));}
struct Point {int x,y;};
// Analytic oscillator-shape preview; not sampled codec output or a live scope.
inline std::array<Point,65> Shape(stackchan_instruments::Waveform waveform){
 std::array<Point,65> points{};for(unsigned i=0;i<65;++i){const unsigned phase=(i%32)*2048;const int raw=waveform==stackchan_instruments::Waveform::Square?(phase<32768?32767:-32768):(phase<32768?int(phase)*2-32768:98303-int(phase)*2);points[i]={int(i)*189/64,22-raw*20/32768};}return points;
}
}
