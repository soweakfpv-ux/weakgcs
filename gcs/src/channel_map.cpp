#include "channel_map.h"

#include <charconv>
#include <sstream>

ChannelMap defaultChannelMap() {
    ChannelMap map{};
    for (int i = 0; i < 8; ++i) map[i] = {ChannelSource::Axis, i, false, crsf::CHANNEL_MID};
    return map;
}

static bool parseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && ptr == s.data() + s.size();
}

bool parseChannelMap(const std::string& text, ChannelMap& out, std::string& error) {
    ChannelMap map{};
    std::istringstream in(text);
    std::string token;
    size_t ch = 0;
    while (in >> token) {
        if (ch >= map.size()) {
            error = "more than 16 channels";
            return false;
        }
        ChannelSource src;
        bool invert = token[0] == '-';
        std::string body = invert ? token.substr(1) : token;
        int n = 0;
        if (body.size() < 2 || !parseInt(body.substr(1), n) || n < 0) {
            error = "invalid channel token '" + token + "'";
            return false;
        }
        switch (body[0]) {
            case 'a': src = {ChannelSource::Axis, n, invert, crsf::CHANNEL_MID}; break;
            case 'b': src = {ChannelSource::Button, n, false, crsf::CHANNEL_MID}; break;
            case 'c': src = {ChannelSource::Constant, 0, false, (uint16_t)n}; break;
            case 'u': src = {ChannelSource::Constant, 0, false, (uint16_t)crsf::usToTicks(n)}; break;
            default: error = "invalid channel token '" + token + "'"; return false;
        }
        if (invert && src.kind != ChannelSource::Axis) {
            error = "only axes can be inverted: '" + token + "'";
            return false;
        }
        if (src.kind == ChannelSource::Constant && (src.value < crsf::CHANNEL_MIN || src.value > crsf::CHANNEL_MAX)) {
            error = "constant out of range: '" + token + "'";
            return false;
        }
        map[ch++] = src;
    }
    out = map;
    return true;
}

uint16_t axisToTicks(int16_t value) {
    // -32768 -> 172, 0 -> 992, 32767 -> 1811
    return (uint16_t)(crsf::CHANNEL_MID + (int32_t)value * 820 / 32768);
}

void applyChannelMap(const ChannelMap& map, const JoystickState& js, uint16_t out[crsf::NUM_CHANNELS]) {
    for (size_t i = 0; i < map.size(); ++i) {
        const ChannelSource& src = map[i];
        switch (src.kind) {
            case ChannelSource::Constant:
                out[i] = src.value;
                break;
            case ChannelSource::Axis:
                if ((size_t)src.index < js.axes.size()) {
                    int v = js.axes[src.index];
                    if (src.invert) v = -v - 1;  // keeps -32768..32767 symmetric
                    out[i] = axisToTicks((int16_t)v);
                } else {
                    out[i] = crsf::CHANNEL_MID;
                }
                break;
            case ChannelSource::Button:
                bool pressed = (size_t)src.index < js.buttons.size() && js.buttons[src.index];
                out[i] = pressed ? crsf::CHANNEL_MAX : crsf::CHANNEL_MIN;
                break;
        }
    }
}
