#include "fractal_core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace fractal {
namespace {

constexpr double kEscapeRadiusSquared = 4.0;
constexpr double kPi = 3.141592653589793238462643383279502884;

struct GradientStop {
    double position;
    Color color;
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double wrap01(double value) {
    value -= std::floor(value);
    return value < 0.0 ? value + 1.0 : value;
}

std::uint8_t lerp_channel(std::uint8_t a, std::uint8_t b, double t) {
    const double value = static_cast<double>(a) +
                         (static_cast<double>(b) - static_cast<double>(a)) * t;
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

Color lerp_color(const Color& a, const Color& b, double t) {
    t = clamp01(t);
    return Color{
        lerp_channel(a.r, b.r, t),
        lerp_channel(a.g, b.g, t),
        lerp_channel(a.b, b.b, t),
        lerp_channel(a.a, b.a, t)};
}

template <std::size_t N>
Color sample_gradient(const std::array<GradientStop, N>& stops, double t) {
    t = clamp01(t);
    if (t <= stops.front().position) {
        return stops.front().color;
    }
    for (std::size_t i = 1; i < stops.size(); ++i) {
        if (t <= stops[i].position) {
            const double span = stops[i].position - stops[i - 1].position;
            const double local = span > 0.0 ? (t - stops[i - 1].position) / span : 0.0;
            return lerp_color(stops[i - 1].color, stops[i].color, local);
        }
    }
    return stops.back().color;
}

Color hsv_to_rgb(double h, double s, double v) {
    h = wrap01(h) * 6.0;
    s = clamp01(s);
    v = clamp01(v);

    const int sector = static_cast<int>(std::floor(h));
    const double f = h - std::floor(h);
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - s * f);
    const double t = v * (1.0 - s * (1.0 - f));

    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    switch (sector % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return Color{
        static_cast<std::uint8_t>(std::lround(r * 255.0)),
        static_cast<std::uint8_t>(std::lround(g * 255.0)),
        static_cast<std::uint8_t>(std::lround(b * 255.0)),
        255};
}

bool is_inside_mandelbrot_cardioid_or_bulb(double cr, double ci) {
    const double x = cr - 0.25;
    const double q = x * x + ci * ci;
    if (q * (q + x) <= 0.25 * ci * ci) {
        return true;
    }

    const double bulbX = cr + 1.0;
    return bulbX * bulbX + ci * ci <= 0.0625;
}

int polynomial_power(FractalKind kind) {
    return kind == FractalKind::Multibrot3 ? 3 : 2;
}

Color inside_color(PaletteKind palette) {
    switch (palette) {
        case PaletteKind::Ocean: return Color{0, 3, 10, 255};
        case PaletteKind::Aurora: return Color{2, 0, 8, 255};
        case PaletteKind::Monochrome: return Color{0, 0, 0, 255};
        default: return Color{0, 0, 0, 255};
    }
}

Color palette_spectrum(double smooth) {
    const double hue = wrap01(0.62 + smooth / 92.0);
    const double envelope = 1.0 - std::exp(-0.055 * std::max(smooth, 0.0));
    const double shimmer = 0.82 + 0.18 *
        (0.5 - 0.5 * std::cos(2.0 * kPi * wrap01(smooth / 46.0)));
    const double value = clamp01(0.035 + 0.965 * envelope * shimmer);
    return hsv_to_rgb(hue, 0.90, value);
}

Color palette_fire(double smooth) {
    static constexpr std::array<GradientStop, 7> stops{{
        {0.00, {2, 0, 8, 255}},
        {0.14, {26, 2, 38, 255}},
        {0.32, {103, 8, 34, 255}},
        {0.52, {214, 43, 16, 255}},
        {0.70, {255, 132, 18, 255}},
        {0.88, {255, 231, 123, 255}},
        {1.00, {255, 255, 245, 255}},
    }};
    return sample_gradient(stops, wrap01(smooth / 84.0));
}

Color palette_ocean(double smooth) {
    static constexpr std::array<GradientStop, 7> stops{{
        {0.00, {0, 3, 16, 255}},
        {0.16, {0, 18, 61, 255}},
        {0.34, {0, 61, 112, 255}},
        {0.52, {0, 132, 165, 255}},
        {0.70, {18, 199, 190, 255}},
        {0.88, {141, 238, 224, 255}},
        {1.00, {240, 255, 252, 255}},
    }};
    return sample_gradient(stops, wrap01(smooth / 104.0));
}

Color palette_aurora(double smooth) {
    static constexpr std::array<GradientStop, 8> stops{{
        {0.00, {4, 0, 18, 255}},
        {0.13, {36, 10, 78, 255}},
        {0.28, {75, 21, 128, 255}},
        {0.44, {12, 102, 144, 255}},
        {0.60, {0, 184, 133, 255}},
        {0.75, {101, 230, 137, 255}},
        {0.90, {229, 241, 153, 255}},
        {1.00, {255, 247, 230, 255}},
    }};
    return sample_gradient(stops, wrap01(smooth / 116.0));
}

Color palette_classic(double smooth) {
    const double t = wrap01(smooth / 76.0);
    const double oneMinusT = 1.0 - t;
    const double r = 9.0 * oneMinusT * t * t * t;
    const double g = 15.0 * oneMinusT * oneMinusT * t * t;
    const double b = 8.5 * oneMinusT * oneMinusT * oneMinusT * t;
    return Color{
        static_cast<std::uint8_t>(std::lround(clamp01(r) * 255.0)),
        static_cast<std::uint8_t>(std::lround(clamp01(g) * 255.0)),
        static_cast<std::uint8_t>(std::lround(clamp01(b) * 255.0)),
        255};
}

Color palette_monochrome(double smooth) {
    const double phase = wrap01(smooth / 58.0);
    const double wave = 0.5 - 0.5 * std::cos(2.0 * kPi * phase);
    const double value = std::pow(wave, 0.72);
    const auto c = static_cast<std::uint8_t>(std::lround(value * 255.0));
    return Color{c, c, c, 255};
}

}  // namespace

std::string_view fractal_name(FractalKind kind) {
    switch (kind) {
        case FractalKind::Mandelbrot: return "Mandelbrot";
        case FractalKind::Julia: return "Julia";
        case FractalKind::BurningShip: return "Burning Ship";
        case FractalKind::Tricorn: return "Tricorn";
        case FractalKind::Multibrot3: return "Multibrot z^3+c";
        case FractalKind::Celtic: return "Celtic";
        case FractalKind::Count: break;
    }
    return "Unknown";
}

std::string_view palette_name(PaletteKind kind) {
    switch (kind) {
        case PaletteKind::Spectrum: return "Spectrum";
        case PaletteKind::Fire: return "Fire";
        case PaletteKind::Ocean: return "Ocean";
        case PaletteKind::Aurora: return "Aurora";
        case PaletteKind::Classic: return "Classic";
        case PaletteKind::Monochrome: return "Monochrome";
        case PaletteKind::Count: break;
    }
    return "Unknown";
}

IterationResult iterate_point(
    FractalKind kind,
    double pixelRe,
    double pixelIm,
    double juliaCRe,
    double juliaCIm,
    int maxIterations) {

    maxIterations = std::max(maxIterations, 1);

    double zr = 0.0;
    double zi = 0.0;
    double cr = pixelRe;
    double ci = pixelIm;

    if (kind == FractalKind::Julia) {
        zr = pixelRe;
        zi = pixelIm;
        cr = juliaCRe;
        ci = juliaCIm;
    } else if (kind == FractalKind::Mandelbrot &&
               is_inside_mandelbrot_cardioid_or_bulb(cr, ci)) {
        return IterationResult{false, maxIterations, 0.0, 0.0, 0.0,
                               static_cast<double>(maxIterations)};
    }

    const int power = polynomial_power(kind);

    for (int i = 0; i < maxIterations; ++i) {
        double nextRe = 0.0;
        double nextIm = 0.0;

        switch (kind) {
            case FractalKind::Mandelbrot:
            case FractalKind::Julia: {
                const double zr2 = zr * zr;
                const double zi2 = zi * zi;
                nextRe = zr2 - zi2 + cr;
                nextIm = 2.0 * zr * zi + ci;
                break;
            }
            case FractalKind::BurningShip: {
                const double ar = std::abs(zr);
                const double ai = std::abs(zi);
                nextRe = ar * ar - ai * ai + cr;
                nextIm = 2.0 * ar * ai + ci;
                break;
            }
            case FractalKind::Tricorn: {
                const double zr2 = zr * zr;
                const double zi2 = zi * zi;
                nextRe = zr2 - zi2 + cr;
                nextIm = -2.0 * zr * zi + ci;
                break;
            }
            case FractalKind::Multibrot3: {
                const double zr2 = zr * zr;
                const double zi2 = zi * zi;
                nextRe = zr * (zr2 - 3.0 * zi2) + cr;
                nextIm = zi * (3.0 * zr2 - zi2) + ci;
                break;
            }
            case FractalKind::Celtic: {
                const double zr2 = zr * zr;
                const double zi2 = zi * zi;
                nextRe = std::abs(zr2 - zi2) + cr;
                nextIm = 2.0 * zr * zi + ci;
                break;
            }
            case FractalKind::Count:
                break;
        }

        zr = nextRe;
        zi = nextIm;
        const double magnitudeSquared = zr * zr + zi * zi;
        if (magnitudeSquared > kEscapeRadiusSquared) {
            const int iterations = i + 1;
            const double logAbs = 0.5 * std::log(magnitudeSquared);
            double smooth = static_cast<double>(iterations);
            if (logAbs > 0.0) {
                smooth = static_cast<double>(iterations) + 1.0 -
                         std::log(logAbs) / std::log(static_cast<double>(power));
            }
            return IterationResult{
                true, iterations, zr, zi, magnitudeSquared, smooth};
        }
    }

    return IterationResult{
        false, maxIterations, zr, zi, zr * zr + zi * zi,
        static_cast<double>(maxIterations)};
}

Color color_from_iteration(PaletteKind palette, const IterationResult& result) {
    if (!result.escaped) {
        return inside_color(palette);
    }

    const double smooth = result.smoothIterations;
    switch (palette) {
        case PaletteKind::Spectrum: return palette_spectrum(smooth);
        case PaletteKind::Fire: return palette_fire(smooth);
        case PaletteKind::Ocean: return palette_ocean(smooth);
        case PaletteKind::Aurora: return palette_aurora(smooth);
        case PaletteKind::Classic: return palette_classic(smooth);
        case PaletteKind::Monochrome: return palette_monochrome(smooth);
        case PaletteKind::Count: break;
    }
    return Color{255, 0, 255, 255};
}

void render_rgba(const RenderRequest& request, std::vector<std::uint8_t>& rgba) {
    if (request.width <= 0 || request.height <= 0) {
        rgba.clear();
        return;
    }

    const int samplesPerAxis = std::clamp(request.samplesPerAxis, 1, 4);
    const int sampleCount = samplesPerAxis * samplesPerAxis;
    const std::size_t requiredSize =
        static_cast<std::size_t>(request.width) *
        static_cast<std::size_t>(request.height) * 4U;
    rgba.resize(requiredSize);

    const double halfWidth = static_cast<double>(request.width) * 0.5;
    const double halfHeight = static_cast<double>(request.height) * 0.5;

#if defined(FRACTAL_USE_OPENMP)
#pragma omp parallel for schedule(static)
#endif
    for (int py = 0; py < request.height; ++py) {
        for (int px = 0; px < request.width; ++px) {
            unsigned int sumR = 0;
            unsigned int sumG = 0;
            unsigned int sumB = 0;
            unsigned int sumA = 0;

            for (int sy = 0; sy < samplesPerAxis; ++sy) {
                for (int sx = 0; sx < samplesPerAxis; ++sx) {
                    const double offsetX =
                        (static_cast<double>(sx) + 0.5) /
                        static_cast<double>(samplesPerAxis);
                    const double offsetY =
                        (static_cast<double>(sy) + 0.5) /
                        static_cast<double>(samplesPerAxis);

                    const double x = static_cast<double>(px) + offsetX;
                    const double y = static_cast<double>(py) + offsetY;
                    const double re = request.centerRe +
                                      (x - halfWidth) * request.unitsPerPixel;
                    const double im = request.centerIm +
                                      (halfHeight - y) * request.unitsPerPixel;

                    const IterationResult result = iterate_point(
                        request.fractal,
                        re,
                        im,
                        request.juliaCRe,
                        request.juliaCIm,
                        request.maxIterations);
                    const Color color = color_from_iteration(request.palette, result);
                    sumR += color.r;
                    sumG += color.g;
                    sumB += color.b;
                    sumA += color.a;
                }
            }

            const std::size_t index =
                (static_cast<std::size_t>(py) * static_cast<std::size_t>(request.width) +
                 static_cast<std::size_t>(px)) * 4U;
            rgba[index + 0] = static_cast<std::uint8_t>(sumR / sampleCount);
            rgba[index + 1] = static_cast<std::uint8_t>(sumG / sampleCount);
            rgba[index + 2] = static_cast<std::uint8_t>(sumB / sampleCount);
            rgba[index + 3] = static_cast<std::uint8_t>(sumA / sampleCount);
        }
    }
}

}  // namespace fractal
