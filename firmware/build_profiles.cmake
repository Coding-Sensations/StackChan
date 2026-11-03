# Resolve once before project/component discovery. IDF's early scan runs in a
# fresh cmake -P process with no inherited cache/selection variables. Both normal
# components and that child must LOAD the parent's canonical result, not resolve.
if(COMMAND idf_build_get_property)
    idf_build_get_property(profile_build_dir BUILD_DIR)
    if(NOT profile_build_dir OR NOT EXISTS "${profile_build_dir}/stackchan-profile/feature-selection.cmake")
        message(FATAL_ERROR "Canonical parent build-profile receipt is missing; reconfigure from project root")
    endif()
    include("${profile_build_dir}/stackchan-profile/feature-selection.cmake")
    return()
elseif(CMAKE_BUILD_EARLY_EXPANSION)
    message(FATAL_ERROR "Early feature selection requires IDF's exported BUILD_DIR")
endif()
if(NOT DEFINED STACKCHAN_BUILD_PROFILE)
    set(STACKCHAN_BUILD_PROFILE "full")
endif()
set(STACKCHAN_BUILD_PROFILE "${STACKCHAN_BUILD_PROFILE}" CACHE STRING "Pre-flash profile: full, no-games, mira-core, custom")
set_property(CACHE STACKCHAN_BUILD_PROFILE PROPERTY STRINGS full no-games mira-core custom)
if(NOT DEFINED STACKCHAN_FEATURE_MANIFEST)
    set(STACKCHAN_FEATURE_MANIFEST "")
endif()
set(STACKCHAN_FEATURE_MANIFEST "${STACKCHAN_FEATURE_MANIFEST}" CACHE FILEPATH "Schema-1 JSON selection for custom profile")
set(profile_tool "${CMAKE_CURRENT_LIST_DIR}/tools/build_profile.py")
if(NOT PYTHON)
    find_program(PYTHON NAMES python3 REQUIRED)
endif()
set(selection_args)
if(STACKCHAN_FEATURE_MANIFEST)
    list(APPEND selection_args --selection "${STACKCHAN_FEATURE_MANIFEST}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${STACKCHAN_FEATURE_MANIFEST}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${profile_tool}")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/main/build_profiles/muse-boundary.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/main/build_profiles/muse-boundary.json"
        "${CMAKE_CURRENT_LIST_DIR}/main/apps/stock_shell/provider_companion_app.cc"
        "${CMAKE_CURRENT_LIST_DIR}/main/companion/provider_room_view.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/main/feature_profiles/muse_disabled_boundary.cc"
        "${CMAKE_CURRENT_LIST_DIR}/components/muse_direct_protocol/CMakeLists.txt")
endif()
execute_process(COMMAND "${PYTHON}" "${profile_tool}"
    --profile "${STACKCHAN_BUILD_PROFILE}" ${selection_args}
    --main-root "${CMAKE_CURRENT_LIST_DIR}/main"
    --output "${CMAKE_BINARY_DIR}/stackchan-profile"
    RESULT_VARIABLE profile_result OUTPUT_VARIABLE profile_output ERROR_VARIABLE profile_error)
if(NOT profile_result EQUAL 0)
    message(FATAL_ERROR "${profile_error}")
endif()
message(STATUS "${profile_output}")
include("${CMAKE_BINARY_DIR}/stackchan-profile/feature-selection.cmake")
