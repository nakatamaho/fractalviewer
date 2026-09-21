<!-- SPDX-License-Identifier: BSD-2-Clause -->
<!-- Copyright (c) 2026 Nakata Maho -->

# SDL3 Fractal Explorer

A cross-platform C++17 fractal explorer for macOS, Windows, and Linux.
It uses SDL3 for the window/renderer, explicit real/imaginary arithmetic for all
fractal formulas, and OpenMP for CPU parallelism when the compiler supports it.
No complex-number class is used.

SDL3 3.4.14 is pinned as the default FetchContent dependency. A compatible
system SDL3 can be selected explicitly with `FRACTAL_USE_SYSTEM_SDL3=ON`.

## Included fractals

1. Mandelbrot
2. Julia
3. Burning Ship
4. Tricorn
5. Multibrot `z^3 + c`
6. Celtic

## Palette presets

- Spectrum
- Fire
- Ocean
- Aurora
- Classic polynomial coloring
- Monochrome

Palettes and coloring algorithms are independent. You can use any palette with
any of the four coloring modes below.

A reference render using the Aurora palette is included at
`docs/previews/coloring_modes_aurora.png`.

## Build on Linux

The project uses CMake and C++17. The default Release build downloads the
pinned SDL3 3.4.14 source and links SDL3 statically, so the resulting binary
does not need `libSDL3.so` at runtime. Linux platform libraries remain system
dependencies.

Build the release binary with:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/fractalviewer
```

To intentionally use an externally installed system SDL3 instead, configure
with `-DFRACTAL_USE_SYSTEM_SDL3=ON` and, if needed,
`-DCMAKE_PREFIX_PATH=/path/to/SDL3`. OpenMP is enabled automatically when the
compiler provides it.

### MinGW cross-build

The same default FetchContent path builds SDL3 as a static library for MinGW.
The MinGW C and C++ runtimes are statically linked as well:

```sh
sudo apt install -y g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel
```

The result is `build-mingw/fractalviewer.exe`. Windows system DLLs remain
normal platform dependencies; SDL3, OpenMP, libgcc, and libstdc++ are linked
into the executable.

## License

BSD 2-Clause License. See [LICENSE](LICENSE).

## Coloring modes

### Smooth

Continuous escape-time coloring. This removes the obvious integer iteration
bands of a basic escape-time renderer and remains the fastest general mode.

### Distance

Derivative-based distance-estimation shading. For Mandelbrot, Julia, and
Multibrot this uses the usual derivative magnitude, expressed internally as a
2x2 real Jacobian so no complex-number type is needed. The estimated distance
is measured relative to the current pixel scale before it is used for shading,
which keeps the appearance useful while zooming.

Burning Ship, Tricorn, and Celtic are non-holomorphic / piecewise maps. For
those variants the program propagates their real 2x2 Jacobian and uses its
spectral norm. The resulting quantity is a practical local distance estimate,
not a claim of the same analytic distance formula available for holomorphic
quadratic maps.

### Orbit Trap

Tracks the minimum distance of the orbit to a geometric trap and maps that
metric through the selected palette. Three trap presets are included:

- Cross: distance to the real or imaginary axis
- Circle: distance to the circle `|z| = 0.5`
- Point: distance to the point `0.5 + 0i`

Orbit-trap evaluation intentionally disables the Mandelbrot cardioid/bulb
shortcut because the orbit itself is needed for coloring, including bounded
points.

### Histogram

Two-pass histogram equalization of the smooth escape values. The first pass
computes the escape metric for the whole image, then a 4096-bin cumulative
distribution remaps the available palette range, and the second pass applies
the equalized colors. This often reveals much more structure in areas where
ordinary smooth coloring concentrates most pixels into a narrow color range.

The expensive escape calculation and final pixel mapping remain OpenMP
parallel. Histogram construction itself is a small linear pass over the image.

## Controls

| Input | Action |
|---|---|
| Mouse wheel | Zoom around the cursor |
| Left drag | Pan |
| Double left click | Zoom in 2x around cursor |
| Right click | Zoom out 2x around cursor |
| `1` ... `6` | Select fractal |
| `F` or `Tab` | Next fractal |
| `P` or `Space` | Next palette |
| `M` | Next coloring mode: Smooth / Distance / Orbit Trap / Histogram |
| `T` | Next orbit-trap preset: Cross / Circle / Point |
| `A` | Toggle 1x1 / 2x2 supersampling |
| `[` / `]` | Base iteration limit -/+ 100 |
| `R` | Reset current fractal view |
| `J` | Switch to Julia / cycle Julia presets |
| `C` | Use the current cursor coordinate as Julia `c` |
| `H` | Show / hide the menu and HUD |
| `Esc` or `Q` | Quit |

The overlay shows the complex-plane coordinate under the mouse, current pixel
scale, zoom factor, effective iteration limit, coloring mode, palette, AA
setting, OpenMP state, render time, and the selected orbit trap when relevant.

## Build: Linux

With GCC or Clang and CMake/Ninja installed:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/fractal_explorer
```

GCC normally provides OpenMP through `libgomp`. If OpenMP is not found, the
program still builds and runs with a serial fallback.

## Build: macOS

Install build tools and OpenMP:

```bash
brew install cmake ninja libomp
```

First try Apple Clang directly:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOpenMP_ROOT="$(brew --prefix libomp)"
cmake --build build
ctest --test-dir build --output-on-failure
./build/fractal_explorer
```

If CMake still cannot detect OpenMP with Apple Clang, Homebrew LLVM is the most
predictable route:

```bash
brew install llvm libomp
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" \
  -DOpenMP_ROOT="$(brew --prefix libomp)"
cmake --build build
ctest --test-dir build --output-on-failure
./build/fractal_explorer
```

## Build: Windows / Visual Studio

From a Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\fractal_explorer.exe
```

MSVC OpenMP is used automatically when CMake's `FindOpenMP` detects it.

## Use a system SDL3 instead of downloading SDL3

```bash
cmake -S . -B build \
  -DFRACTAL_FETCH_SDL=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/sdl3/prefix
```

## Build and test only the calculation core

The project can be tested on a machine without SDL3:

```bash
cmake -S . -B build-core \
  -DFRACTAL_BUILD_GUI=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

This is useful for numerical CI and for checking OpenMP independently of the
window-system dependency.

## Disable OpenMP deliberately

```bash
cmake -S . -B build -DFRACTAL_ENABLE_OPENMP=OFF
```

## Rendering notes

- Rendering is CPU-side; SDL3 uploads the completed RGBA image to a streaming
  texture and displays it through the platform renderer.
- High-DPI mode is enabled. Mouse events are converted to renderer coordinates
  so cursor-anchored zooming remains correct on Retina and scaled displays.
- Double precision is used. Extremely deep zooms eventually hit floating-point
  precision limits; arbitrary-precision perturbation rendering is intentionally
  outside this compact version.
- 2x2 supersampling is optional because it costs roughly 4x the orbit work.
- Histogram mode needs an additional image-sized metric buffer and a second
  color-mapping pass.
- Smooth and Histogram modes retain the Mandelbrot main-cardioid / period-2
  bulb shortcut. Orbit Trap cannot use that shortcut because it needs the
  actual orbit.

## Numerical QA

`tests/core_tests.cpp` checks:

- bounded and escaped reference points,
- positive finite distance estimates for regular escaped Mandelbrot/Tricorn
  points,
- distinct orbit-trap metrics,
- all four coloring render paths,
- opaque RGBA output,
- 2x2 histogram supersampling,
- and output-size invariants.
