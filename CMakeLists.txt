cmake_minimum_required(VERSION 3.24)

project(fractalviewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL3 is an external dependency. Install SDL3 separately and point CMake at
# its prefix with -DCMAKE_PREFIX_PATH=/path/to/prefix when it is not installed
# in a standard system prefix.
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

find_package(OpenMP QUIET)

add_executable(fractalviewer
    main.cpp
    fractal_core.cpp
    fractal_core.hpp
)

target_compile_features(fractalviewer PRIVATE cxx_std_17)
target_compile_options(fractalviewer PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:CXX,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

target_link_libraries(fractalviewer PRIVATE SDL3::SDL3)

if(OpenMP_CXX_FOUND)
    target_compile_definitions(fractalviewer PRIVATE FRACTAL_USE_OPENMP=1)
    target_link_libraries(fractalviewer PRIVATE OpenMP::OpenMP_CXX)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS fractalviewer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
