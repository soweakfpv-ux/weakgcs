# Platform-independent core (no SDL): config, channel mapping, telemetry decoding. Unit-tested.
add_library(wgcs_core STATIC
    src/channel_map.cpp
    src/config.cpp
    src/telemetry.cpp
)
target_include_directories(wgcs_core PUBLIC src)
target_link_libraries(wgcs_core PUBLIC wgcs_common)
if(MSVC)
    target_compile_options(wgcs_core PUBLIC /W4 /utf-8)
else()
    target_compile_options(wgcs_core PUBLIC -Wall -Wextra)
endif()

if(NOT WGCS_BUILD_APP)
    return()
endif()

find_package(SDL3 3.2 CONFIG QUIET)
if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not installed, fetching it (static build)")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.30
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_executable(weakgcs
    src/link.cpp
    src/main.cpp
    src/osd.cpp
    src/platform.cpp
    src/udp_socket.cpp
    src/video.cpp
)
target_link_libraries(weakgcs PRIVATE wgcs_core SDL3::SDL3)
if(WIN32)
    target_link_libraries(weakgcs PRIVATE ws2_32 winmm)
    target_compile_definitions(weakgcs PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(weakgcs PRIVATE Threads::Threads)
endif()

# Default config next to the executable.
add_custom_command(TARGET weakgcs POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/gcs.ini $<TARGET_FILE_DIR:weakgcs>/gcs.ini
)
