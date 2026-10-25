#include "video.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

constexpr Uint64 REOPEN_INTERVAL_MS = 2000;

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                          [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
    return it != haystack.end();
}

double specFps(const SDL_CameraSpec& s) {
    return s.framerate_denominator ? (double)s.framerate_numerator / s.framerate_denominator : 0;
}

// Requested size wins; then highest frame rate (lower latency per frame); then larger size.
bool betterSpec(const SDL_CameraSpec& a, const SDL_CameraSpec& b, const Config& cfg) {
    auto sizeMatch = [&](const SDL_CameraSpec& s) {
        return (cfg.videoWidth == 0 || s.width == cfg.videoWidth) && (cfg.videoHeight == 0 || s.height == cfg.videoHeight);
    };
    auto fpsMatch = [&](const SDL_CameraSpec& s) { return cfg.videoFps == 0 || (int)(specFps(s) + 0.5) == cfg.videoFps; };
    if (sizeMatch(a) != sizeMatch(b)) return sizeMatch(a);
    if (fpsMatch(a) != fpsMatch(b)) return fpsMatch(a);
    if (specFps(a) != specFps(b)) return specFps(a) > specFps(b);
    return a.width * a.height > b.width * b.height;
}

}  // namespace

Video::~Video() { close(); }

void Video::close() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (camera_) SDL_CloseCamera(camera_);
    texture_ = nullptr;
    camera_ = nullptr;
    cameraId_ = 0;
    stats_ = {};
}

void Video::tryOpen() {
    int count = 0;
    SDL_CameraID* ids = SDL_GetCameras(&count);
    for (int i = 0; ids && i < count && !camera_; ++i) {
        const char* name = SDL_GetCameraName(ids[i]);
        std::string n = name ? name : "";
        if (!cfg_.videoDevice.empty() && !containsIgnoreCase(n, cfg_.videoDevice)) continue;

        int numSpecs = 0;
        SDL_CameraSpec** specs = SDL_GetCameraSupportedFormats(ids[i], &numSpecs);
        const SDL_CameraSpec* best = nullptr;
        for (int s = 0; specs && s < numSpecs; ++s) {
            if (!best || betterSpec(*specs[s], *best, cfg_)) best = specs[s];
        }
        camera_ = SDL_OpenCamera(ids[i], best);
        SDL_free(specs);

        if (!camera_) {
            std::printf("[video] cannot open %s: %s\n", n.c_str(), SDL_GetError());
            continue;
        }
        cameraId_ = ids[i];
        stats_.deviceName = n;
        if (best) {
            std::printf("[video] %s: %dx%d @ %.2f fps, %s\n", n.c_str(), best->width, best->height, specFps(*best),
                        SDL_GetPixelFormatName(best->format));
        } else {
            std::printf("[video] %s: default format\n", n.c_str());
        }
    }
    SDL_free(ids);
}

void Video::onEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_CAMERA_DEVICE_REMOVED && camera_ && e.cdevice.which == cameraId_) {
        std::printf("[video] device removed\n");
        close();
    } else if (e.type == SDL_EVENT_CAMERA_DEVICE_DENIED && camera_ && e.cdevice.which == cameraId_) {
        std::printf("[video] access denied by the OS\n");
        close();
    } else if (e.type == SDL_EVENT_CAMERA_DEVICE_ADDED && !camera_) {
        lastOpenAttempt_ = 0;  // try right away
    }
}

bool Video::update(SDL_Renderer* renderer) {
    Uint64 now = SDL_GetTicks();
    if (!camera_) {
        if (lastOpenAttempt_ == 0 || now - lastOpenAttempt_ >= REOPEN_INTERVAL_MS) {
            lastOpenAttempt_ = now;
            tryOpen();
        }
        if (!camera_) return false;
    }
    if (SDL_GetCameraPermissionState(camera_) <= 0) return false;  // pending (or denied: DENIED event follows)

    // Drain the queue and keep only the newest frame: showing an older one is pure latency.
    SDL_Surface* frame = nullptr;
    Uint64 ts = 0;
    while (SDL_Surface* f = SDL_AcquireCameraFrame(camera_, &ts)) {
        if (frame) SDL_ReleaseCameraFrame(camera_, frame);
        frame = f;
        ++fpsFrames_;
    }
    if (!frame) return false;

    if (!texture_ || texture_->w != frame->w || texture_->h != frame->h || texture_->format != frame->format) {
        if (texture_) SDL_DestroyTexture(texture_);
        texture_ = SDL_CreateTexture(renderer, frame->format, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
        if (!texture_) std::printf("[video] cannot create texture: %s\n", SDL_GetError());
    }
    if (texture_) SDL_UpdateTexture(texture_, nullptr, frame->pixels, frame->pitch);
    stats_.open = true;
    stats_.width = frame->w;
    stats_.height = frame->h;
    SDL_ReleaseCameraFrame(camera_, frame);

    if (fpsWindowStart_ == 0) fpsWindowStart_ = now;
    if (now - fpsWindowStart_ >= 1000) {
        stats_.fps = fpsFrames_ * 1000.0 / (double)(now - fpsWindowStart_);
        fpsFrames_ = 0;
        fpsWindowStart_ = now;
    }
    return true;
}

void Video::draw(SDL_Renderer* renderer) const {
    if (!texture_) return;
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &outW, &outH);
    // Letterbox, keeping the source aspect ratio.
    float scale = std::min((float)outW / texture_->w, (float)outH / texture_->h);
    SDL_FRect dst;
    dst.w = texture_->w * scale;
    dst.h = texture_->h * scale;
    dst.x = (outW - dst.w) / 2;
    dst.y = (outH - dst.h) / 2;
    SDL_RenderTexture(renderer, texture_, nullptr, &dst);
}
