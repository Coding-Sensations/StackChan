cmake_minimum_required(VERSION 3.16)

project(stackchan_firmware_tests LANGUAGES CXX)

enable_testing()

include(HostPolicyTests.cmake)

add_executable(motion_math_test
    motion_math_test.cpp
    ../main/stackchan/motion/motion_math.cpp
)

target_include_directories(motion_math_test PRIVATE
    ../main
)

target_compile_features(motion_math_test PRIVATE cxx_std_17)

add_test(NAME motion_math_test COMMAND $<TARGET_FILE:motion_math_test>)

add_executable(motion_input_test
    motion_input_test.cpp
    ../main/stackchan/motion/motion.cpp
    ../main/stackchan/motion/motion_math.cpp
)
target_include_directories(motion_input_test PRIVATE motion-fixtures ../main)
target_compile_features(motion_input_test PRIVATE cxx_std_17)
add_test(NAME motion_input_test COMMAND $<TARGET_FILE:motion_input_test>)

add_executable(category_test navigation/category_test.cpp)
add_executable(raster_test navigation/raster_test.cpp)
add_executable(catalog_bounded_test navigation/catalog_bounded_test.cpp
    ../main/apps/app_launcher/view/effect_catalog/effect_catalog.cpp)
add_executable(runtime_stop_test navigation/runtime_stop_test.cpp)
foreach(target category_test raster_test catalog_bounded_test runtime_stop_test)
    target_include_directories(${target} PRIVATE ../main)
    target_compile_features(${target} PRIVATE cxx_std_17)
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
endforeach()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME screensaver_consumers_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/screensaver_consumers_test.py)
add_test(NAME launcher_navigation_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/launcher_test.py)
add_test(NAME generated_teardown_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/teardown_test.py)
set_tests_properties(generated_teardown_test PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME protocol_lifetime_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/protocol_lifetime_test.py)
set_tests_properties(protocol_lifetime_test PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME ble_stop_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/ble_stop_test.py)
add_executable(station_handoff_test navigation/station_handoff_test.cpp)
target_include_directories(station_handoff_test PRIVATE ../main)
target_compile_features(station_handoff_test PRIVATE cxx_std_17)
add_test(NAME station_handoff_test COMMAND station_handoff_test)
add_test(NAME station_sdk_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/station_sdk_test.py)
add_test(NAME phone_station_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/phone_station_test.py)
add_test(NAME station_event_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/station_event_test.py)
add_test(NAME reconciliation_network_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/reconciliation_network_test.py)
add_test(NAME reconciliation_phone_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/reconciliation_phone_test.py)
add_test(NAME reconciliation_vendor_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/reconciliation_vendor_test.py)
add_test(NAME reconciliation_power_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/reconciliation_power_test.py)
add_test(NAME host_dependency_preparation_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/host_dependency_preparation_test.py)
add_test(NAME installer_cancellation_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/installer_cancellation_test.py)
add_test(NAME installer_session_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/installer_session_test.py)

add_executable(screensaver_management_test navigation/screensaver_management_test.cpp)
target_include_directories(screensaver_management_test PRIVATE ../main)
target_compile_features(screensaver_management_test PRIVATE cxx_std_17)
add_test(NAME screensaver_management_test COMMAND screensaver_management_test)
add_test(NAME screensaver_management_ui_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/screensaver_management_ui_test.py)

add_test(NAME nested_usb_close_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/nested_usb_close_test.py)

add_test(NAME usb_actor_close_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/usb_actor_close_test.py)

add_test(NAME companion_entry_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/companion_entry_test.py)

add_executable(moire_removal_test navigation/moire_removal_test.cpp
    ../main/apps/app_launcher/view/effect_catalog/effect_catalog.cpp)
target_include_directories(moire_removal_test PRIVATE ../main)
target_compile_features(moire_removal_test PRIVATE cxx_std_17)
add_test(NAME moire_removal_test COMMAND moire_removal_test)

add_test(NAME companion_home_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/companion_home_test.py)

add_test(NAME companion_layout_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/companion_layout_test.py)
add_test(NAME companion_native_layout_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/companion_native_layout_test.py)
add_test(NAME companion_status_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/companion_status_test.py)

add_test(NAME espnow_stop_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/espnow_stop_test.py)

add_test(NAME provider_companion_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/provider_companion_test.py)

add_test(NAME provider_room_view_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation/provider_room_view_test.py)

# Portable static contracts: no runtime authority, device or external artwork.
add_test(NAME adapter_boundary_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_adapter_boundary.py)
add_test(NAME portrait_geometry_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_portrait_geometry.py)
add_test(NAME voice_profile_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_voice_profile.py)
add_test(NAME voice_context_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_voice_context.py)
add_test(NAME voice_api_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_voice_api.py)

add_test(NAME bounded_profile_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_bounded_profile.py)

add_test(NAME voice_bridge_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_voice_bridge.py)

add_test(NAME voice_handoff_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_voice_handoff.py)

add_test(NAME voice_exchange_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_voice_exchange.py)

add_test(NAME muse_codec_test COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/test_muse_codec.py)

add_test(NAME muse_pcm_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_pcm.py)

add_test(NAME voice_worker_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_worker.py)

add_test(NAME voice_admission_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_admission.py)

stackchan_host_test(voice_lease_current_test voice_lease_current_test.cpp)
add_test(NAME voice_core_codec_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_core_codec.py)

add_test(NAME muse_dma_discard_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_dma_discard.py)

add_test(NAME muse_amp_reset_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_amp_reset.py)

add_test(NAME voice_review_permit_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_review_permit.py)

add_test(NAME voice_session_owner_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_session_owner.py)

add_test(NAME voice_device_consent_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_device_consent.py)

add_test(NAME voice_device_runtime_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_device_runtime.py)

add_test(NAME voice_ui_owner_handoff_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_ui_owner_handoff.py)

add_test(NAME voice_task_owner_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_task_owner.py)
add_test(NAME voice_task_reaper_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_task_reaper.py)

add_test(NAME voice_setup_issuer_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_setup_issuer.py)
add_test(NAME voice_core_lookup_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_core_lookup.py)

add_test(NAME voice_native_hold_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_voice_native_hold.py)

add_test(NAME muse_amplifier_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_amplifier.py)

add_test(NAME muse_quiesce_diagnostic_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_quiesce_diagnostic.py)

add_test(NAME muse_cold_output_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_cold_output.py)

add_test(NAME muse_local_diagnostic_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_local_diagnostic.py)
add_test(NAME muse_diagnostic_build_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_diagnostic_build.py)

add_test(NAME muse_diagnostic_startup_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_muse_diagnostic_startup.py)
