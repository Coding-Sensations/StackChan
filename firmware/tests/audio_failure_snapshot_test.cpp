#include "audio/audio_failure_snapshot.h"

#include <cassert>
#include <string>

int main() {
    audio::failure::Snapshot snapshot;
    snapshot.stage = "task-create-audio-input";
    snapshot.status = -1;
    snapshot.owner = companion::AudioOwner::Stock;
    snapshot.purpose = companion::AudioPurpose::StockRuntime;
    snapshot.generation = 42;
    snapshot.initialized = true;
    snapshot.service_stopped = false;
    snapshot.active_workers = 2;
    snapshot.input_sample_rate = 48000;
    snapshot.output_sample_rate = 24000;
    snapshot.input_channels = 2;
    snapshot.input_enabled = true;
    snapshot.output_enabled = false;
    snapshot.free_heap = 111;
    snapshot.min_free_heap = 112;
    snapshot.internal_free = 113;
    snapshot.internal_largest = 114;
    snapshot.dma_free = 115;
    snapshot.dma_largest = 116;
    snapshot.psram_free = 117;
    snapshot.psram_largest = 118;
    snapshot.current_stack_hwm = 119;
    snapshot.input_stack_hwm = 120;
    snapshot.output_stack_hwm = 121;
    snapshot.codec_stack_hwm = 122;

    char buffer[768];
    const std::size_t line_length = audio::failure::FormatSnapshot(buffer, sizeof(buffer), snapshot);
    const std::string line(buffer, line_length);
    assert(line.find("stage=task-create-audio-input") != std::string::npos);
    assert(line.find("status=-1") != std::string::npos);
    assert(line.find("owner=stock") != std::string::npos);
    assert(line.find("purpose=stock-runtime") != std::string::npos);
    assert(line.find("generation=42") != std::string::npos);
    assert(line.find("heap_free=111") != std::string::npos);
    assert(line.find("internal_largest=114") != std::string::npos);
    assert(line.find("dma_largest=116") != std::string::npos);
    assert(line.find("psram_largest=118") != std::string::npos);
    assert(line.find("stack_codec=122") != std::string::npos);
    return 0;
}
