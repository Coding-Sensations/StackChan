#pragma once
#include <string>
namespace stock_tools {
// Fixed original pack IDs 0..7; read at most96KiB under existing board SD/display
// lease, with fresh CID before/after. Invoke only while instrument rendering is
// stopped; never a realtime/audio callback. No caller-controlled path or writes.
bool ReadStackChanInstrumentSample(unsigned sample_id,std::string& output);
}
