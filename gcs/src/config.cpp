#include "config.h"

#include <charconv>
#include <fstream>
#include <sstream>

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool toInt(const std::string& s, int& out) {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return !s.empty() && ec == std::errc() && ptr == s.data() + s.size();
}

bool toFloat(const std::string& s, float& out) {
    try {
        size_t used = 0;
        out = std::stof(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

bool toBool(const std::string& s, bool& out) {
    if (s == "1" || s == "true" || s == "yes" || s == "on") return out = true, true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return out = false, true;
    return false;
}

bool setValue(Config& cfg, const std::string& section, const std::string& key, const std::string& value,
              std::string& error) {
    auto intIn = [&](int& field, int lo, int hi) {
        if (!toInt(value, field) || field < lo || field > hi) {
            error = "expected integer " + std::to_string(lo) + ".." + std::to_string(hi);
            return false;
        }
        return true;
    };
    std::string k = section + "." + key;

    if (k == "network.mast_ip") return cfg.mastIp = value, true;
    if (k == "network.mast_port") {
        int port = 0;
        if (!intIn(port, 1, 65535)) return false;
        cfg.mastPort = (uint16_t)port;
        return true;
    }
    if (k == "rc.rate_hz") return intIn(cfg.rcRateHz, 50, 1000);
    if (k == "rc.joystick") return cfg.joystickName = value, true;
    if (k == "rc.arm_channel") return intIn(cfg.armChannel, 0, 16);
    if (k == "rc.channels") return parseChannelMap(value, cfg.channelMap, error);
    if (k == "video.device") return cfg.videoDevice = value, true;
    if (k == "video.width") return intIn(cfg.videoWidth, 0, 8192);
    if (k == "video.height") return intIn(cfg.videoHeight, 0, 8192);
    if (k == "video.fps") return intIn(cfg.videoFps, 0, 1000);
    if (k == "video.vsync") return toBool(value, cfg.vsync) || (error = "expected 0/1", false);
    if (k == "video.fullscreen") return toBool(value, cfg.fullscreen) || (error = "expected 0/1", false);
    if (k == "osd.battery_cells") return intIn(cfg.batteryCells, 0, 14);
    if (k == "osd.cell_warn_voltage") return toFloat(value, cfg.cellWarnVoltage) || (error = "expected number", false);
    if (k == "osd.lq_warn") return intIn(cfg.lqWarn, 0, 100);

    error = "unknown key";
    return false;
}

}  // namespace

bool parseConfig(const std::string& text, Config& cfg, std::string& error) {
    std::istringstream in(text);
    std::string line, section;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        size_t comment = line.find_first_of(";#");
        if (comment != std::string::npos) line.resize(comment);
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        size_t eq = line.find('=');
        std::string err;
        if (eq == std::string::npos) {
            err = "expected key = value";
        } else {
            std::string key = trim(line.substr(0, eq));
            if (setValue(cfg, section, key, trim(line.substr(eq + 1)), err)) continue;
            err = "[" + section + "] " + key + ": " + err;
        }
        error = "line " + std::to_string(lineNo) + ": " + err;
        return false;
    }
    return true;
}

bool loadConfig(const std::string& path, Config& cfg, std::string& error) {
    std::ifstream f(path);
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    if (!parseConfig(ss.str(), cfg, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}
