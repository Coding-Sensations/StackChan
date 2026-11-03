# Source-only frozen cohort; deliberately do not add upstream component directories.
include("${CMAKE_CURRENT_LIST_DIR}/sources.cmake")
if(NOT CMAKE_BUILD_EARLY_EXPANSION AND NOT CONFIG_IDF_TARGET_ESP32S3)
    message(FATAL_ERROR "mira_wake pinned ESP-NN assembly supports ESP32-S3 only")
endif()
idf_component_register(
    SRCS ${MIRA_TFLM_SOURCES} ${MIRA_ESP_NN_SOURCES} ${MIRA_FRONTEND_SOURCES}
    INCLUDE_DIRS
        "third_party"
        "third_party/espressif__esp-tflite-micro"
        "third_party/espressif__esp-tflite-micro/third_party/gemmlowp"
        "third_party/espressif__esp-tflite-micro/third_party/flatbuffers/include"
        "third_party/espressif__esp-tflite-micro/third_party/ruy"
        "third_party/espressif__esp-tflite-micro/third_party/kissfft"
        "third_party/espressif__esp-nn/include"
        "third_party/esphome__esp-micro-speech-features/include"
    PRIV_INCLUDE_DIRS
        "third_party/espressif__esp-tflite-micro/signal/src"
        "third_party/espressif__esp-nn/src/common"
        "third_party/esphome__esp-micro-speech-features/src"
    PRIV_REQUIRES esp_timer driver)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC TF_LITE_STATIC_MEMORY PRIVATE ESP_NN TF_LITE_DISABLE_X86_NEON)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -O3 -fno-unwind-tables -ffunction-sections -fdata-sections
        -Wno-error=attributes -Wno-error=shadow -Wno-maybe-uninitialized
        -Wno-error=sign-compare -Wno-error=double-promotion
        -Wno-error=stringop-overread -Wno-type-limits -Wno-nonnull -Wno-unused-function
        $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17;-fno-exceptions;-fno-rtti;-fno-threadsafe-statics>)
    set_source_files_properties(${MIRA_ESP_NN_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2;-fno-unroll-loops")
    set_source_files_properties("third_party/espressif__esp-tflite-micro/tensorflow/lite/micro/kernels/esp_nn/depthwise_conv.cc" PROPERTIES COMPILE_OPTIONS "-O2")
    target_link_libraries(${COMPONENT_LIB} PRIVATE m)
endif()
# Asset/provider/factory availability remains false in native integration headers.
