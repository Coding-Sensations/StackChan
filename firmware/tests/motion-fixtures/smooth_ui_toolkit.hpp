#pragma once
// Host-only animation boundary; no clock, peripheral, or IO implementation.
namespace uitk {
struct Vector2i { int x=0, y=0; Vector2i()=default; Vector2i(int a,int b):x(a),y(b){} };
struct AnimateValue {};
struct SpringOptions_t {};
}
