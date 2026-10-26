// weakgcs: FPV ground station. Radio (USB HID) -> laptop -> Ethernet -> mast (ELRS TX + VRX).
//
// Keys: F11 fullscreen, F1 OSD on/off, F2 channel monitor, Esc quit,
//       Ctrl+Left/Right video channel, Ctrl+Up/Down video band (if vrx_channel is set).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <string>

#include "config.h"
#include "link.h"
#include "osd.h"
#include "platform.h"
#include "udp_socket.h"
#include "video.h"

namespace {

constexpr Uint64 OSD_REFRESH_MS = 50;  // redraw at least this often even without new video frames

// Ctrl+arrows: step the video channel within the band, or the band keeping the channel number.
uint8_t stepVrxChannel(uint8_t current, SDL_Keycode key) {
    if (current >= vrx::NUM_CHANNELS) return current;
    int band = current / vrx::CHANNELS_PER_BAND, ch = current % vrx::CHANNELS_PER_BAND;
    switch (key) {
        case SDLK_LEFT: ch = (ch + vrx::CHANNELS_PER_BAND - 1) % vrx::CHANNELS_PER_BAND; break;
        case SDLK_RIGHT: ch = (ch + 1) % vrx::CHANNELS_PER_BAND; break;
        case SDLK_DOWN: band = (band + vrx::NUM_BANDS - 1) % vrx::NUM_BANDS; break;
        case SDLK_UP: band = (band + 1) % vrx::NUM_BANDS; break;
        default: return current;
    }
    return (uint8_t)(band * vrx::CHANNELS_PER_BAND + ch);
}

std::string defaultConfigPath() {
    const char* base = SDL_GetBasePath();  // directory of the executable
    return std::string(base ? base : "") + "gcs.ini";
}

}  // namespace

int main(int argc, char** argv) {
    std::string configPath = argc > 1 ? argv[1] : defaultConfigPath();
    Config cfg;
    std::string error;
    if (!loadConfig(configPath, cfg, error)) {
        std::fprintf(stderr, "config: %s\n", error.c_str());
        return 1;
    }
    std::printf("config: %s\n", configPath.c_str());

    // The RC thread samples the joystick itself, right before each packet (see link.cpp).
    SDL_SetHint(SDL_HINT_AUTO_UPDATE_JOYSTICKS, "0");
    // Keep reading the radio when the window is not focused.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_CAMERA)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    platform::init();

    UdpSocket sock;
    if (!sock.open(cfg.mastIp, cfg.mastPort, 100, error)) {
        std::fprintf(stderr, "network: %s\n", error.c_str());
        return 1;
    }
    std::printf("mast: %s:%u, RC %d Hz\n", cfg.mastIp.c_str(), cfg.mastPort, cfg.rcRateHz);

    SDL_Window* window = SDL_CreateWindow("weakgcs", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                                                    (cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "window: %s\n", SDL_GetError());
        return 1;
    }
    // VSync off by default: a new video frame is shown immediately (may tear) instead of
    // waiting up to one refresh period.
    SDL_SetRenderVSync(renderer, cfg.vsync ? 1 : 0);
    std::printf("renderer: %s, vsync %s\n", SDL_GetRendererName(renderer), cfg.vsync ? "on" : "off");

    Link link(cfg, sock);
    link.start();
    Video video(cfg);
    OsdOptions osd;
    Uint64 lastRender = 0;
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                switch (e.key.key) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_F11:
                        SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
                        break;
                    case SDLK_F1: osd.visible = !osd.visible; break;
                    case SDLK_F2: osd.channelMonitor = !osd.channelMonitor; break;
                    case SDLK_LEFT:
                    case SDLK_RIGHT:
                    case SDLK_UP:
                    case SDLK_DOWN:
                        if (e.key.mod & SDL_KMOD_CTRL) {
                            link.setVrxChannel(stepVrxChannel(link.snapshot().vrxRequested, e.key.key));
                        }
                        break;
                    default: break;
                }
            }
            video.onEvent(e);
        }

        bool newFrame = video.update(renderer);
        Uint64 now = SDL_GetTicks();
        if (!newFrame && now - lastRender < OSD_REFRESH_MS) {
            SDL_DelayPrecise(500 * 1000);  // poll for frames every 0.5 ms
            continue;
        }
        lastRender = now;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        video.draw(renderer);
        drawOsd(renderer, cfg, osd, link.snapshot(), video.stats());
        SDL_RenderPresent(renderer);
    }

    link.stop();
    sock.close();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    platform::shutdown();
    SDL_Quit();
    return 0;
}
