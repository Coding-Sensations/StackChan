#pragma once
namespace companion::header_layout {
struct Rect {int x,y,width,height;};
// Secondary actions are relative to Details' y44 panel. The header remains
// reserved for Details/Face and the wrapper's More/Back navigation.
inline constexpr Rect Export{8,8,148,44},Status{164,8,148,44},Remember{8,60,188,44},Forget{204,60,108,44},Mute{8,112,96,44},Room{112,112,96,44},LocalWake{216,112,96,44};
constexpr bool Overlaps(Rect a,Rect b){return a.x<b.x+b.width&&a.x+a.width>b.x&&a.y<b.y+b.height&&a.y+a.height>b.y;}
}
