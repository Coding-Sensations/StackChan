# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.6.0")
add_definitions(-DFIRMWARE_VERSION=\"${PROJECT_VER}\")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

# Auto-load a local sdkconfig overlay (git-ignored) when present. Lets
# self-hosted / custom deployments pin CONFIG_STACKCHAN_SERVER_URL,
# CONFIG_OTA_URL, etc. without touching committed defaults.
# Manual equivalent: SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.local" idf.py build
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults.local")
    set(SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults.local"
        CACHE STRING "ESP-IDF sdkconfig default files (auto-extended with local overlay)" FORCE)
    message(STATUS "StackChan: detected sdkconfig.defaults.local, applying overlay")
endif()

# Offline qualification may use an already reviewed component cache. No download
# or cache mutation is performed by this option; local components take priority.
set(STACKCHAN_OFFLINE_COMPONENT_ROOT "" CACHE PATH "Reviewed offline IDF component cache")
if(STACKCHAN_OFFLINE_COMPONENT_ROOT)
    file(GLOB STACKCHAN_OFFLINE_COMPONENTS LIST_DIRECTORIES true "${STACKCHAN_OFFLINE_COMPONENT_ROOT}/*")
    foreach(STACKCHAN_OFFLINE_COMPONENT IN LISTS STACKCHAN_OFFLINE_COMPONENTS)
        if(EXISTS "${STACKCHAN_OFFLINE_COMPONENT}/CMakeLists.txt")
            list(APPEND EXTRA_COMPONENT_DIRS "${STACKCHAN_OFFLINE_COMPONENT}")
        endif()
    endforeach()
endif()

# Resolve before component discovery so optional components can register empty.
include("${CMAKE_CURRENT_SOURCE_DIR}/build_profiles.cmake")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stack-chan)

# An explicitly pinned local graph may restore manager-injected edges when a
# reviewed cache is built offline. This option never downloads or overrides SDK code.
set(STACKCHAN_OFFLINE_GRAPH "" CACHE FILEPATH "Pinned generated offline dependency links")
if(STACKCHAN_OFFLINE_GRAPH)
    if(NOT "$ENV{IDF_COMPONENT_MANAGER}" STREQUAL "0" OR MINIMAL_BUILD OR COMPONENTS OR EXCLUDE_COMPONENTS)
        message(FATAL_ERROR "Offline graph requires manager disabled and the complete registered component set")
    endif()
    include("${STACKCHAN_OFFLINE_GRAPH}")
endif()
