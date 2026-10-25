#pragma once

#include <cstdint>
#include <string>

#include <weakgcs/protocol.h>

#include "channel_map.h"

struct Config {
    // [network]
    std::string mastIp = "192.168.77.2";
    uint16_t mastPort = wgcs::MAST_PORT;

    // [rc]
    int rcRateHz = 500;
    std::string joystickName;  // case-insensitive substring; empty = first joystick
    int armChannel = 5;        // 1-based; 0 disables the arm guard
    ChannelMap channelMap = defaultChannelMap();

    // [video]
    std::string videoDevice;  // case-insensitive substring; empty = first camera
    int videoWidth = 0;       // 0 = pick automatically
    int videoHeight = 0;
    int videoFps = 0;
    bool vsync = false;
    bool fullscreen = false;

    // [osd]
    int batteryCells = 0;  // 0 = detect from voltage
    float cellWarnVoltage = 3.5f;
    int lqWarn = 70;
};

// Reads an INI file over the defaults in `cfg`. Unknown keys are errors, to catch typos.
bool loadConfig(const std::string& path, Config& cfg, std::string& error);
bool parseConfig(const std::string& text, Config& cfg, std::string& error);
