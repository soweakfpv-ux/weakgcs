cmake_minimum_required(VERSION 3.21)
project(weakgcs LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(WGCS_BUILD_APP "Build the GCS application (needs SDL3, fetched automatically if not installed)" ON)
option(WGCS_BUILD_TESTS "Build unit tests" ON)

# Protocol headers shared with the mast firmware.
add_library(wgcs_common INTERFACE)
target_include_directories(wgcs_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_subdirectory(gcs)

if(WGCS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
