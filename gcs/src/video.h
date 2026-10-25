// Video from a capture device (USB grabber fed by the VRX on the mast), shown with minimal latency:
// always the newest frame, no buffering, no decoding (SDL picks a raw format like YUY2/NV12).
#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "config.h"

struct VideoStats {
    bool open = false;
    std::string deviceName;
    int width = 0, height = 0;
    double fps = 0;
};

class Video {
public:
    explicit Video(const Config& cfg) : cfg_(cfg) {}
    ~Video();

    // Grabs the newest frame into the texture. Returns true if a new frame arrived.
    bool update(SDL_Renderer* renderer);
    void draw(SDL_Renderer* renderer) const;
    void onEvent(const SDL_Event& e);
    const VideoStats& stats() const { return stats_; }

private:
    void tryOpen();
    void close();

    const Config& cfg_;
    SDL_Camera* camera_ = nullptr;
    SDL_CameraID cameraId_ = 0;
    SDL_Texture* texture_ = nullptr;
    Uint64 lastOpenAttempt_ = 0;
    Uint64 fpsWindowStart_ = 0;
    int fpsFrames_ = 0;
    VideoStats stats_;
};
