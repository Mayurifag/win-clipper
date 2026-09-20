cmake_minimum_required(VERSION 3.20)

project(win_clipper VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(clipper_core
    src/config.cpp
    src/config_storage.cpp
)

target_include_directories(clipper_core PUBLIC src)

add_executable(win-clipper WIN32
    src/capture_agent.cpp
    src/capture_agent_capture.cpp
    src/main.cpp
    src/resources.rc
    src/settings_window.cpp
    src/settings_window_lists.cpp
    src/startup.cpp
    src/window_catalog.cpp
)

target_link_libraries(win-clipper PRIVATE
    clipper_core
    advapi32
    comctl32
    dwmapi
    user32
    uxtheme
)

target_compile_definitions(win-clipper PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(win-clipper PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(win-clipper PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(MINGW)
        target_link_options(win-clipper PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
endif()
