#pragma once
#include <cstdint>
namespace effect_catalog {
uint16_t Count();
const char* Name(uint16_t index);
bool Usable(uint16_t index);
void Reset(uint16_t index);
// Single engine; caller owns framebuffer. No hardware, blocking waits, or IO.
bool Render(uint16_t index, uint32_t elapsed_ms, uint16_t* pixels, uint16_t width=320, uint16_t height=240, uint64_t (*clock)()=nullptr, uint32_t work_budget=1000000);
}
