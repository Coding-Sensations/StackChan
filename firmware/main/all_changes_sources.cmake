# Apply after original configuration choices and Stock AI generation.
list(REMOVE_ITEM SOURCES "${XIAOZHI_MAIN_DIR}/audio/audio_service.cc")
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_service.cc")
list(REMOVE_ITEM SOURCES "${XIAOZHI_MAIN_DIR}/audio/processors/afe_audio_processor.cc")
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio/processors/afe_audio_processor.cc")
list(REMOVE_ITEM SOURCES "${XIAOZHI_MAIN_DIR}/audio/wake_words/afe_wake_word.cc")
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio/wake_words/afe_wake_word.cc")
list(REMOVE_ITEM SOURCES "${XIAOZHI_MAIN_DIR}/audio/wake_words/custom_wake_word.cc")
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio/wake_words/custom_wake_word.cc")
# The patched Stock AI application is authoritative; keep other generated units.
if(STACKCHAN_COMPANION_BOOT AND NOT CMAKE_BUILD_EARLY_EXPANSION)
    list(REMOVE_ITEM SOURCES "${STOCK_BACKEND_DIR}/stock_application.cc")
endif()
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/companion/stock_application.cc")
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/backend_link_reference.cc" "${CMAKE_CURRENT_SOURCE_DIR}/factory_target_probe.cc")
# USB transport sources are outside the original main glob.
file(GLOB ALL_CHANGES_USB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/usb_composite_v2/*.cc")
list(APPEND SOURCES ${ALL_CHANGES_USB_SOURCES})
list(REMOVE_DUPLICATES SOURCES)
# Put reviewed local headers ahead of fetched Xiaozhi headers.
list(PREPEND INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/audio" "${CMAKE_CURRENT_SOURCE_DIR}/audio/wake_words" "${CMAKE_CURRENT_SOURCE_DIR}/audio/processors")
# The checked hardware-owner header borrows the existing endpoint factory.
list(APPEND INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/companion/wake_endpoint")
