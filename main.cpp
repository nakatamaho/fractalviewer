#include "fractal_core.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

#if defined(FRACTAL_USE_OPENMP)
#include <omp.h>
#endif

namespace {

struct ViewState {
    double centerRe{-0.5};
    double centerIm{0.0};
    double unitsPerPixel{3.0 / 900.0};
    double resetUnitsPerPixel{3.0 / 900.0};
};

struct AppState {
    fractal::FractalKind fractal{fractal::FractalKind::Mandelbrot};
    fractal::PaletteKind palette{fractal::PaletteKind::Spectrum};
    ViewState view{};
    int baseIterations{400};
    int samplesPerAxis{1};
    double juliaCRe{-0.8};
    double juliaCIm{0.156};
    int juliaPresetIndex{0};
    float mouseX{0.0F};
    float mouseY{0.0F};
    bool dragging{false};
    float lastDragX{0.0F};
    float lastDragY{0.0F};
    bool showHelp{true};
    double lastRenderMilliseconds{0.0};
};

constexpr std::array<std::array<double, 2>, 5> kJuliaPresets{{
    {-0.8, 0.156},
    {-0.4, 0.6},
    {0.285, 0.01},
    {-0.835, -0.2321},
    {-0.7269, 0.1889},
}};

int enum_count(fractal::FractalKind) {
    return static_cast<int>(fractal::FractalKind::Count);
}

int enum_count(fractal::PaletteKind) {
    return static_cast<int>(fractal::PaletteKind::Count);
}

fractal::FractalKind next_fractal(fractal::FractalKind kind) {
    const int next = (static_cast<int>(kind) + 1) % enum_count(kind);
    return static_cast<fractal::FractalKind>(next);
}

fractal::PaletteKind next_palette(fractal::PaletteKind kind) {
    const int next = (static_cast<int>(kind) + 1) % enum_count(kind);
    return static_cast<fractal::PaletteKind>(next);
}

int openmp_thread_count() {
#if defined(FRACTAL_USE_OPENMP)
    return omp_get_max_threads();
#else
    return 1;
#endif
}

std::string_view openmp_status() {
#if defined(FRACTAL_USE_OPENMP)
    return "ON";
#else
    return "OFF";
#endif
}

void reset_view(AppState& state, int width, int height) {
    const int safeHeight = std::max(height, 1);
    double span = 3.0;

    switch (state.fractal) {
        case fractal::FractalKind::Mandelbrot:
            state.view.centerRe = -0.5;
            state.view.centerIm = 0.0;
            span = 3.0;
            break;
        case fractal::FractalKind::Julia:
            state.view.centerRe = 0.0;
            state.view.centerIm = 0.0;
            span = 3.2;
            break;
        case fractal::FractalKind::BurningShip:
            state.view.centerRe = -0.45;
            state.view.centerIm = -0.5;
            span = 3.0;
            break;
        case fractal::FractalKind::Tricorn:
            state.view.centerRe = 0.0;
            state.view.centerIm = 0.0;
            span = 3.4;
            break;
        case fractal::FractalKind::Multibrot3:
            state.view.centerRe = 0.0;
            state.view.centerIm = 0.0;
            span = 3.0;
            break;
        case fractal::FractalKind::Celtic:
            state.view.centerRe = -0.4;
            state.view.centerIm = 0.0;
            span = 3.2;
            break;
        case fractal::FractalKind::Count:
            break;
    }

    state.view.unitsPerPixel = span / static_cast<double>(safeHeight);
    state.view.resetUnitsPerPixel = state.view.unitsPerPixel;
    state.mouseX = static_cast<float>(std::max(width, 1)) * 0.5F;
    state.mouseY = static_cast<float>(safeHeight) * 0.5F;
}

void screen_to_plane(
    const AppState& state,
    int width,
    int height,
    double screenX,
    double screenY,
    double& re,
    double& im) {

    re = state.view.centerRe +
         (screenX - static_cast<double>(width) * 0.5) * state.view.unitsPerPixel;
    im = state.view.centerIm +
         (static_cast<double>(height) * 0.5 - screenY) * state.view.unitsPerPixel;
}

void zoom_at(
    AppState& state,
    int width,
    int height,
    double mouseX,
    double mouseY,
    double factor) {

    factor = std::clamp(factor, 0.02, 50.0);

    double beforeRe = 0.0;
    double beforeIm = 0.0;
    screen_to_plane(state, width, height, mouseX, mouseY, beforeRe, beforeIm);

    state.view.unitsPerPixel = std::max(state.view.unitsPerPixel * factor, 1.0e-18);

    double afterRe = 0.0;
    double afterIm = 0.0;
    screen_to_plane(state, width, height, mouseX, mouseY, afterRe, afterIm);

    state.view.centerRe += beforeRe - afterRe;
    state.view.centerIm += beforeIm - afterIm;
}

int effective_iterations(const AppState& state) {
    const double ratio = state.view.resetUnitsPerPixel /
                         std::max(state.view.unitsPerPixel, 1.0e-300);
    const double zoomOctaves = ratio > 1.0 ? std::log2(ratio) : 0.0;
    const int extra = static_cast<int>(std::lround(zoomOctaves * 12.0));
    return std::clamp(state.baseIterations + extra, 64, 5000);
}

double zoom_factor(const AppState& state) {
    return state.view.resetUnitsPerPixel /
           std::max(state.view.unitsPerPixel, 1.0e-300);
}

SDL_Texture* create_texture(SDL_Renderer* renderer, int width, int height) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    return SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);
}

bool upload_pixels(
    SDL_Texture* texture,
    const std::vector<std::uint8_t>& rgba,
    int width,
    int height) {

    if (!texture || width <= 0 || height <= 0) {
        return false;
    }

    void* destination = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(texture, nullptr, &destination, &pitch)) {
        return false;
    }

    const auto* source = rgba.data();
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4U;
    auto* destinationBytes = static_cast<std::uint8_t*>(destination);
    for (int y = 0; y < height; ++y) {
        std::memcpy(
            destinationBytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(pitch),
            source + static_cast<std::size_t>(y) * rowBytes,
            rowBytes);
    }

    SDL_UnlockTexture(texture);
    return true;
}

bool render_fractal(
    AppState& state,
    SDL_Texture* texture,
    int width,
    int height,
    std::vector<std::uint8_t>& rgba) {

    fractal::RenderRequest request;
    request.width = width;
    request.height = height;
    request.centerRe = state.view.centerRe;
    request.centerIm = state.view.centerIm;
    request.unitsPerPixel = state.view.unitsPerPixel;
    request.maxIterations = effective_iterations(state);
    request.samplesPerAxis = state.samplesPerAxis;
    request.fractal = state.fractal;
    request.palette = state.palette;
    request.juliaCRe = state.juliaCRe;
    request.juliaCIm = state.juliaCIm;

    const auto start = std::chrono::steady_clock::now();
    fractal::render_rgba(request, rgba);
    const auto end = std::chrono::steady_clock::now();
    state.lastRenderMilliseconds =
        std::chrono::duration<double, std::milli>(end - start).count();

    return upload_pixels(texture, rgba, width, height);
}

void draw_text_line(SDL_Renderer* renderer, float x, float y, const char* text) {
    SDL_RenderDebugText(renderer, x, y, text);
}

void draw_hud(
    SDL_Window* window,
    SDL_Renderer* renderer,
    const AppState& state,
    int width,
    int height) {

    double cursorRe = 0.0;
    double cursorIm = 0.0;
    screen_to_plane(state, width, height, state.mouseX, state.mouseY, cursorRe, cursorIm);

    float uiScale = SDL_GetWindowDisplayScale(window);
    if (!(uiScale > 0.0F)) {
        uiScale = 1.0F;
    }
    uiScale = std::max(1.0F, uiScale);

    SDL_SetRenderScale(renderer, uiScale, uiScale);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const float logicalWidth = static_cast<float>(width) / uiScale;
    const float panelHeight = state.showHelp ? 90.0F : 42.0F;
    SDL_FRect panel{6.0F, 6.0F, std::max(20.0F, logicalWidth - 12.0F), panelHeight};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 176);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 245, 248, 255, 255);

    char line[512];
    std::snprintf(
        line,
        sizeof(line),
        "%s | Palette: %s | Iter: %d | AA: %dx%d | OpenMP: %s (%d) | %.1f ms",
        fractal::fractal_name(state.fractal).data(),
        fractal::palette_name(state.palette).data(),
        effective_iterations(state),
        state.samplesPerAxis,
        state.samplesPerAxis,
        openmp_status().data(),
        openmp_thread_count(),
        state.lastRenderMilliseconds);
    draw_text_line(renderer, 12.0F, 11.0F, line);

    std::snprintf(
        line,
        sizeof(line),
        "Cursor: %.15g %+.15gi | units/px: %.4e | zoom: %.4gx",
        cursorRe,
        cursorIm,
        state.view.unitsPerPixel,
        zoom_factor(state));
    draw_text_line(renderer, 12.0F, 22.0F, line);

    if (state.fractal == fractal::FractalKind::Julia) {
        std::snprintf(
            line,
            sizeof(line),
            "Julia c: %.15g %+.15gi | J: next Julia preset | C: c = cursor",
            state.juliaCRe,
            state.juliaCIm);
        draw_text_line(renderer, 12.0F, 33.0F, line);
    } else {
        draw_text_line(renderer, 12.0F, 33.0F, "C: switch to Julia using cursor as c | J: Julia preset");
    }

    if (state.showHelp) {
        draw_text_line(renderer, 12.0F, 48.0F,
                       "Mouse wheel: zoom at cursor | Left drag: pan | Double-left: zoom in | Right: zoom out");
        draw_text_line(renderer, 12.0F, 59.0F,
                       "1..6: fractal | F/Tab: next fractal | P/Space: palette | A: antialias | R: reset");
        draw_text_line(renderer, 12.0F, 70.0F,
                       "[/]: iterations -/+ 100 | H: help | Esc/Q: quit");
    }

    SDL_SetRenderScale(renderer, 1.0F, 1.0F);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void present_scene(
    SDL_Window* window,
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const AppState& state,
    int width,
    int height) {

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (texture) {
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    }
    draw_hud(window, renderer, state, width, height);
    SDL_RenderPresent(renderer);
}

void select_fractal(AppState& state, fractal::FractalKind kind, int width, int height) {
    state.fractal = kind;
    reset_view(state, width, height);
}

void select_julia_preset(AppState& state, int width, int height) {
    state.juliaPresetIndex =
        (state.juliaPresetIndex + 1) % static_cast<int>(kJuliaPresets.size());
    state.juliaCRe = kJuliaPresets[static_cast<std::size_t>(state.juliaPresetIndex)][0];
    state.juliaCIm = kJuliaPresets[static_cast<std::size_t>(state.juliaPresetIndex)][1];
    if (state.fractal != fractal::FractalKind::Julia) {
        select_fractal(state, fractal::FractalKind::Julia, width, height);
    }
}

bool handle_key(
    const SDL_KeyboardEvent& key,
    AppState& state,
    int width,
    int height,
    bool& sceneDirty,
    bool& presentDirty) {

    if (key.repeat) {
        return true;
    }

    switch (key.scancode) {
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_Q:
            return false;
        case SDL_SCANCODE_1:
        case SDL_SCANCODE_2:
        case SDL_SCANCODE_3:
        case SDL_SCANCODE_4:
        case SDL_SCANCODE_5:
        case SDL_SCANCODE_6: {
            const int index = static_cast<int>(key.scancode) - static_cast<int>(SDL_SCANCODE_1);
            select_fractal(state, static_cast<fractal::FractalKind>(index), width, height);
            sceneDirty = true;
            break;
        }
        case SDL_SCANCODE_F:
        case SDL_SCANCODE_TAB:
            select_fractal(state, next_fractal(state.fractal), width, height);
            sceneDirty = true;
            break;
        case SDL_SCANCODE_P:
        case SDL_SCANCODE_SPACE:
            state.palette = next_palette(state.palette);
            sceneDirty = true;
            break;
        case SDL_SCANCODE_A:
            state.samplesPerAxis = state.samplesPerAxis == 1 ? 2 : 1;
            sceneDirty = true;
            break;
        case SDL_SCANCODE_R:
            reset_view(state, width, height);
            sceneDirty = true;
            break;
        case SDL_SCANCODE_H:
            state.showHelp = !state.showHelp;
            presentDirty = true;
            break;
        case SDL_SCANCODE_LEFTBRACKET:
            state.baseIterations = std::max(64, state.baseIterations - 100);
            sceneDirty = true;
            break;
        case SDL_SCANCODE_RIGHTBRACKET:
            state.baseIterations = std::min(5000, state.baseIterations + 100);
            sceneDirty = true;
            break;
        case SDL_SCANCODE_J:
            select_julia_preset(state, width, height);
            sceneDirty = true;
            break;
        case SDL_SCANCODE_C: {
            double re = 0.0;
            double im = 0.0;
            screen_to_plane(state, width, height, state.mouseX, state.mouseY, re, im);
            state.juliaCRe = re;
            state.juliaCIm = im;
            if (state.fractal != fractal::FractalKind::Julia) {
                select_fractal(state, fractal::FractalKind::Julia, width, height);
            }
            sceneDirty = true;
            break;
        }
        default:
            break;
    }

    presentDirty = true;
    return true;
}

}  // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const SDL_WindowFlags flags =
        static_cast<SDL_WindowFlags>(SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!SDL_CreateWindowAndRenderer(
            "SDL3 Fractal Explorer",
            1280,
            900,
            flags,
            &window,
            &renderer)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_CreateWindowAndRenderer failed: %s",
            SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int width = 0;
    int height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &width, &height)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_GetRenderOutputSize failed: %s",
            SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    AppState state;
    reset_view(state, width, height);

    SDL_Texture* texture = create_texture(renderer, width, height);
    if (!texture) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_CreateTexture failed: %s",
            SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<std::uint8_t> rgba;
    bool running = true;
    bool sceneDirty = true;
    bool presentDirty = true;

    while (running) {
        if (sceneDirty) {
            if (!render_fractal(state, texture, width, height, rgba)) {
                SDL_LogError(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "Texture upload failed: %s",
                    SDL_GetError());
                running = false;
                break;
            }
            sceneDirty = false;
            presentDirty = true;
        }

        if (presentDirty) {
            present_scene(window, renderer, texture, state, width, height);
            presentDirty = false;
        }

        SDL_Event event;
        if (!SDL_WaitEvent(&event)) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "SDL_WaitEvent failed: %s",
                SDL_GetError());
            break;
        }

        do {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);

            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                    int newWidth = 0;
                    int newHeight = 0;
                    if (SDL_GetRenderOutputSize(renderer, &newWidth, &newHeight) &&
                        newWidth > 0 && newHeight > 0) {
                        width = newWidth;
                        height = newHeight;
                        SDL_DestroyTexture(texture);
                        texture = create_texture(renderer, width, height);
                        if (!texture) {
                            SDL_LogError(
                                SDL_LOG_CATEGORY_APPLICATION,
                                "Texture resize failed: %s",
                                SDL_GetError());
                            running = false;
                            break;
                        }
                        sceneDirty = true;
                        presentDirty = true;
                    }
                    break;
                }

                case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                    presentDirty = true;
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    state.mouseX = event.motion.x;
                    state.mouseY = event.motion.y;
                    if (state.dragging) {
                        const float dx = event.motion.x - state.lastDragX;
                        const float dy = event.motion.y - state.lastDragY;
                        state.view.centerRe -= static_cast<double>(dx) * state.view.unitsPerPixel;
                        state.view.centerIm += static_cast<double>(dy) * state.view.unitsPerPixel;
                        state.lastDragX = event.motion.x;
                        state.lastDragY = event.motion.y;
                        sceneDirty = true;
                    }
                    presentDirty = true;
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    state.mouseX = event.button.x;
                    state.mouseY = event.button.y;
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (event.button.clicks >= 2) {
                            zoom_at(state, width, height, event.button.x, event.button.y, 0.5);
                            sceneDirty = true;
                        } else {
                            state.dragging = true;
                            state.lastDragX = event.button.x;
                            state.lastDragY = event.button.y;
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        zoom_at(state, width, height, event.button.x, event.button.y, 2.0);
                        sceneDirty = true;
                    }
                    presentDirty = true;
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        state.dragging = false;
                    }
                    presentDirty = true;
                    break;

                case SDL_EVENT_MOUSE_WHEEL: {
                    state.mouseX = event.wheel.mouse_x;
                    state.mouseY = event.wheel.mouse_y;
                    double wheelY = static_cast<double>(event.wheel.y);
                    if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                        wheelY = -wheelY;
                    }
                    if (wheelY != 0.0) {
                        const double factor = std::pow(0.80, wheelY);
                        zoom_at(
                            state,
                            width,
                            height,
                            event.wheel.mouse_x,
                            event.wheel.mouse_y,
                            factor);
                        sceneDirty = true;
                    }
                    presentDirty = true;
                    break;
                }

                case SDL_EVENT_KEY_DOWN:
                    running = handle_key(
                        event.key,
                        state,
                        width,
                        height,
                        sceneDirty,
                        presentDirty);
                    break;

                default:
                    break;
            }
        } while (running && SDL_PollEvent(&event));
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
