// On-screen display drawn over the video: link state, ELRS stats, battery, warnings.
#pragma once

#include <SDL3/SDL.h>

#include "config.h"
#include "link.h"
#include "video.h"

struct OsdOptions {
    bool visible = true;
    bool channelMonitor = false;  // F2: bars for all 16 channels (setup / mapping check)
};

void drawOsd(SDL_Renderer* renderer, const Config& cfg, const OsdOptions& opt, const LinkSnapshot& link,
             const VideoStats& video);
