#include "audio_failure_snapshot.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>
#endif

namespace audio::failure {
namespace {

const char* OwnerName(companion::AudioOwner owner) {
    switch (owner) {
        case companion::AudioOwner::None: return "none";
        case companion::AudioOwner::Mira: return "mira";
        case companion::AudioOwner::Stock: return "stock";
        case companion::AudioOwner::Native: return "native";
        case companion::AudioOwner::DirectUsb: return "direct-usb";
        case companion::AudioOwner::Muse: return "muse";
    }
    return "unknown";
}

const char* PurposeName(companion::AudioPurpose purpose) {
    switch (purpose) {
        case companion::AudioPurpose::Session: return "session";
        case companion::AudioPurpose::StockRuntime: return "stock-runtime";
        case companion::AudioPurpose::NativeRuntime: return "native-runtime";
        case companion::AudioPurpose::DirectUsbSession: return "direct-usb-session";
        case companion::AudioPurpose::LocalWake: return "local-wake";
        case companion::AudioPurpose::PhysicalListen: return "physical-listen";
        case companion::AudioPurpose::MusePushToTalk: return "muse-push-to-talk";
    }
    return "unknown";
}

#if defined(ESP_PLATFORM)
uint32_t StackHighWater(TaskHandle_t task) {
    return task == nullptr ? 0u : static_cast<uint32_t>(uxTaskGetStackHighWaterMark(task));
}
#endif

}  // namespace

std::size_t FormatSnapshot(char* buffer, std::size_t buffer_size, const Snapshot& snapshot) {
    if (buffer == nullptr || buffer_size == 0) {
        return 0;
    }
    const int written = std::snprintf(
        buffer, buffer_size,
        "stage=%s status=%" PRId32 " owner=%s purpose=%s generation=%" PRIu64
        " initialized=%d service_stopped=%d active_workers=%u"
        " codec_in_rate=%d codec_out_rate=%d codec_in_channels=%d"
        " codec_input_enabled=%d codec_output_enabled=%d"
        " heap_free=%zu heap_min=%zu internal_free=%zu internal_largest=%zu"
        " dma_free=%zu dma_largest=%zu psram_free=%zu psram_largest=%zu"
        " stack_current=%" PRIu32 " stack_input=%" PRIu32
        " stack_output=%" PRIu32 " stack_codec=%" PRIu32,
        snapshot.stage ? snapshot.stage : "",
        snapshot.status,
        OwnerName(snapshot.owner),
        PurposeName(snapshot.purpose),
        snapshot.generation,
        snapshot.initialized ? 1 : 0,
        snapshot.service_stopped ? 1 : 0,
        snapshot.active_workers,
        snapshot.input_sample_rate,
        snapshot.output_sample_rate,
        snapshot.input_channels,
        snapshot.input_enabled ? 1 : 0,
        snapshot.output_enabled ? 1 : 0,
        snapshot.free_heap,
        snapshot.min_free_heap,
        snapshot.internal_free,
        snapshot.internal_largest,
        snapshot.dma_free,
        snapshot.dma_largest,
        snapshot.psram_free,
        snapshot.psram_largest,
        snapshot.current_stack_hwm,
        snapshot.input_stack_hwm,
        snapshot.output_stack_hwm,
        snapshot.codec_stack_hwm);
    if (written <= 0) {
        buffer[0] = '\0';
        return 0;
    }
    return std::min<std::size_t>(static_cast<std::size_t>(written), buffer_size - 1);
}

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
                         TaskHandle_t codec_task) {
    Snapshot snapshot;
    snapshot.stage = stage;
    snapshot.status = status;
    snapshot.owner = lease.owner();
    snapshot.purpose = lease.purpose();
    snapshot.generation = lease.generation();
    snapshot.initialized = initialized;
    snapshot.service_stopped = service_stopped;
    snapshot.active_workers = active_workers;
    snapshot.input_sample_rate = input_sample_rate;
    snapshot.output_sample_rate = output_sample_rate;
    snapshot.input_channels = input_channels;
    snapshot.input_enabled = input_enabled;
    snapshot.output_enabled = output_enabled;
    snapshot.free_heap = esp_get_free_heap_size();
    snapshot.min_free_heap = esp_get_minimum_free_heap_size();
    snapshot.internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    snapshot.dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    snapshot.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snapshot.psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    snapshot.current_stack_hwm = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
    snapshot.input_stack_hwm = StackHighWater(input_task);
    snapshot.output_stack_hwm = StackHighWater(output_task);
    snapshot.codec_stack_hwm = StackHighWater(codec_task);
    return snapshot;
}
#endif

}  // namespace audio::failure
