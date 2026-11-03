# Hardware-free policy and production renderer checks. Keep assertions enabled:
# these fixtures use assert(), including calls with side effects.
if(MSVC)
    add_compile_options(/UNDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()
function(stackchan_host_test name source)
    add_executable(${name} ${source} ${ARGN})
    target_include_directories(${name} PRIVATE ../main ../main/apps/app_launcher/view)
    target_compile_features(${name} PRIVATE cxx_std_17)
    add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

stackchan_host_test(advanced_config_test advanced_config_test.cpp)
stackchan_host_test(firmware_install_policy_test firmware_install_policy_test.cpp)
stackchan_host_test(dma_sent_fence_test codec-drain/dma_sent_fence_test.cpp)
stackchan_host_test(camera_authority_test desk-adapters/camera_authority_test.cpp)
stackchan_host_test(camera_wire_lifecycle_test desk-adapters/camera_wire_lifecycle_test.cpp)
stackchan_host_test(screensaver_policy_test desk-adapters/screensaver_policy_test.cpp)
stackchan_host_test(screensaver_catalog_test desk-adapters/screensaver_catalog_test.cpp
    ../main/apps/app_launcher/view/effect_catalog/effect_catalog.cpp)
