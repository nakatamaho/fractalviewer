#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace fractal {

enum class FractalKind {
    Mandelbrot,
    Julia,
    BurningShip,
    Tricorn,
    Multibrot3,
    Celtic,
    Count,
};

enum class PaletteKind {
    Spectrum,
    Fire,
    Ocean,
    Aurora,
    Classic,
    Monochrome,
    Count,
};

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

struct IterationResult {
    bool escaped;
    int iterations;
    double zRe;
    double zIm;
    double magnitudeSquared;
    double smoothIterations;
};

struct RenderRequest {
    int width;
    int height;
    double centerRe;
    double centerIm;
    double unitsPerPixel;
    int maxIterations;
    int samplesPerAxis;
    FractalKind fractal;
    PaletteKind palette;
    double juliaCRe;
    double juliaCIm;
};

std::string_view fractal_name(FractalKind kind);
std::string_view palette_name(PaletteKind kind);

IterationResult iterate_point(
    FractalKind kind,
    double pixelRe,
    double pixelIm,
    double juliaCRe,
    double juliaCIm,
    int maxIterations);

Color color_from_iteration(PaletteKind palette, const IterationResult& result);

void render_rgba(const RenderRequest& request, std::vector<std::uint8_t>& rgba);

}  // namespace fractal
