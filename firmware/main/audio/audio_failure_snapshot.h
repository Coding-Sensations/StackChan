#pragma once

#include <cstddef>
#include <cstdint>

#include "companion/audio_owner.hpp"

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace audio::failure {

struct Snapshot {
    const char* stage = "";
    int32_t status = 0;
    companion::AudioOwner owner = companion::AudioOwner::None;
    companion::AudioPurpose purpose = companion::AudioPurpose::Session;
    uint64_t generation = 0;
    bool initialized = false;
    bool service_stopped = true;
    unsigned active_workers = 0;
    int input_sample_rate = 0;
    int output_sample_rate = 0;
    int input_channels = 0;
    bool input_enabled = false;
    bool output_enabled = false;
    std::size_t free_heap = 0;
    std::size_t min_free_heap = 0;
    std::size_t internal_free = 0;
    std::size_t internal_largest = 0;
    std::size_t dma_free = 0;
    std::size_t dma_largest = 0;
    std::size_t psram_free = 0;
    std::size_t psram_largest = 0;
    uint32_t current_stack_hwm = 0;
    uint32_t input_stack_hwm = 0;
    uint32_t output_stack_hwm = 0;
    uint32_t codec_stack_hwm = 0;
};

std::size_t FormatSnapshot(char* buffer, std::size_t buffer_size, const Snapshot& snapshot);

#if defined(ESP_PLATFORM)
Snapshot CaptureSnapshot(const companion::AudioLease& lease,
                         const char* stage,
                         int32_t status,
                         bool initialized,
                         bool service_stopped,
                         unsigned active_workers,
                         int input_sample_rate,
                         int output_sample_rate,
                         int input_channels,
                         bool input_enabled,
                         bool output_enabled,
                         TaskHandle_t input_task,
                         TaskHandle_t output_task,
                         TaskHandle_t codec_task);
#endif

}  // namespace audio::failure
