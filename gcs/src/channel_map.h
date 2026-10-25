// Maps joystick inputs (the radio in USB HID mode) to the 16 CRSF channels.
//
// The radio does all the mixing (EdgeTX model), so normally this is a 1:1 mapping of
// EdgeTX output channels -> HID axes/buttons -> CRSF channels.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <weakgcs/crsf.h>

struct ChannelSource {
    enum Kind { Constant, Axis, Button };
    Kind kind = Constant;
    int index = 0;        // axis / button number
    bool invert = false;  // axes only
    uint16_t value = crsf::CHANNEL_MID;  // Constant only, CRSF ticks
};

using ChannelMap = std::array<ChannelSource, crsf::NUM_CHANNELS>;

struct JoystickState {
    std::vector<int16_t> axes;
    std::vector<bool> buttons;
};

// ch1..ch8 = axis 0..7, ch9..ch16 = centered.
ChannelMap defaultChannelMap();

// Parses a whitespace-separated list, one token per channel starting at ch1:
//   a3   axis 3        -a3  axis 3 inverted
//   b0   button 0      (pressed = max, released = min)
//   c992 constant (CRSF ticks)      u1500 constant (microseconds)
// Channels not listed stay centered.
bool parseChannelMap(const std::string& text, ChannelMap& out, std::string& error);

uint16_t axisToTicks(int16_t value);

void applyChannelMap(const ChannelMap& map, const JoystickState& js, uint16_t out[crsf::NUM_CHANNELS]);
