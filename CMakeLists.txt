cmake_minimum_required(VERSION 3.24)

project(fractalviewer VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    FRACTAL_USE_SYSTEM_SDL3
    "Use a system SDL3 instead of fetching the pinned SDL3 release"
    OFF)

if(FRACTAL_USE_SYSTEM_SDL3)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
else()
    # Release builds use a pinned, statically linked SDL3 so the application
    # does not require libSDL3.so at runtime. Platform libraries (X11,
    # Wayland, ALSA, etc.) remain normal system dependencies.
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    # XScreenSaver is optional for this renderer and is not present on every
    # minimal Linux development image.
    set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
    # XTest is only used for optional input-injection support.
    set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.14
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

find_package(OpenMP QUIET)

set(FRACTALVIEWER_SOURCES
    main.cpp
    fractal_core.cpp
    fractal_core.hpp
)

if(WIN32)
    add_executable(fractalviewer WIN32 ${FRACTALVIEWER_SOURCES})
else()
    add_executable(fractalviewer ${FRACTALVIEWER_SOURCES})
endif()

target_compile_features(fractalviewer PRIVATE cxx_std_17)
target_compile_options(fractalviewer PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:CXX,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

target_link_libraries(fractalviewer PRIVATE SDL3::SDL3)

if(MINGW)
    # Keep the MinGW runtime self-contained too. Windows system DLLs remain
    # normal platform dependencies, while SDL3 and the C++ runtimes are not
    # required beside the executable.
    target_link_options(fractalviewer PRIVATE -static-libgcc -static-libstdc++)
endif()

if(OpenMP_CXX_FOUND)
    target_compile_definitions(fractalviewer PRIVATE FRACTAL_USE_OPENMP=1)
    target_link_libraries(fractalviewer PRIVATE OpenMP::OpenMP_CXX)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS fractalviewer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
