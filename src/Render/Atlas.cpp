#include "BitMotor/Render/Atlas.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Math/Functions.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>

namespace BitMotor {

namespace Detail {

struct AtlasFrame {
    Rect Area;
    Vec2 Pivot;
    float Density = 1;
};

struct AnimationInfo {
    float Fps = 8;
    bool Loop = true;
    std::vector<int> Ticks;
    std::vector<std::pair<std::string, int>> Events;
};

struct AtlasData {
    std::string Source;
    Texture Image;
    std::vector<AtlasFrame> Frames;
    std::unordered_map<std::string, int> Ids;
    std::unordered_map<std::string, AnimationInfo> Animations;
    std::map<int, std::map<std::string, Socket, std::less<>>> Sockets;
    mutable std::set<std::string, std::less<>> Warned;

    void WarnOnce(std::string_view what, std::string_view name) const {
        std::string key = std::string(what) + ":" + std::string(name);
        if (Warned.insert(key).second) Log::Warn("{}: unknown {} '{}'", Source, what, name);
    }

    Region MakeRegion(int id) const {
        const AtlasFrame& frame = Frames[std::size_t(id)];
        return Region{Image, frame.Area, frame.Pivot, frame.Density, id};
    }
};

} // namespace Detail

namespace {

using Json = nlohmann::json;

// "hero_walk_12" -> ("hero_walk", 12); names without a numeric suffix get -1.
std::pair<std::string, int> SplitNumber(const std::string& name) {
    auto underscore = name.find_last_of('_');
    if (underscore == std::string::npos || underscore + 1 >= name.size()) return {name, -1};
    for (std::size_t i = underscore + 1; i < name.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return {name, -1};
    if (name.size() - underscore > 9) return {name, -1};
    return {name.substr(0, underscore), std::stoi(name.substr(underscore + 1))};
}

float Number(const Json& value, float fallback = 0) { return value.is_number() ? value.get<float>() : fallback; }

int TicksOf(const std::vector<int>& ticks, int frame) {
    return frame >= 0 && frame < int(ticks.size()) ? ticks[std::size_t(frame)] : 1;
}

} // namespace

// ---------------------------------------------------------------------------------------------- Atlas

Atlas Atlas::Parse(std::string_view text, Texture image, std::string_view source) {
    Json json = Json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object() || !json.contains("sprites") || !json["sprites"].is_object()) {
        Log::Error("{}: not an atlas (broken JSON or no \"sprites\")", source);
        return {};
    }
    auto data = std::make_shared<Detail::AtlasData>();
    data->Source = std::string(source);
    data->Image = std::move(image);

    // Frames in a stable order by name and number, so the _0.._N frames of an animation are consecutive.
    std::vector<std::string> names;
    for (auto& [name, value] : json["sprites"].items()) names.push_back(name);
    std::sort(names.begin(), names.end(),
              [](const std::string& a, const std::string& b) { return SplitNumber(a) < SplitNumber(b); });
    for (const std::string& name : names) {
        const Json& v = json["sprites"][name];
        if (!v.is_array() || v.size() < 4) {
            Log::Warn("{}: sprite '{}' needs [x, y, w, h, pivotX, pivotY]", source, name);
            continue;
        }
        Detail::AtlasFrame frame;
        frame.Area = Rect(Number(v[0]), Number(v[1]), Number(v[2]), Number(v[3]));
        if (v.size() >= 6) frame.Pivot = {Number(v[4]), Number(v[5])};
        if (v.size() >= 7) frame.Density = Max(0.01f, Number(v[6], 1));
        data->Ids[name] = int(data->Frames.size());
        data->Frames.push_back(frame);
    }

    // "animations": {"name": fps} or {"name": {"fps", "loop", "durations", "events"}}.
    if (json.contains("animations") && json["animations"].is_object())
        for (auto& [name, v] : json["animations"].items()) {
            Detail::AnimationInfo info;
            if (v.is_number()) {
                info.Fps = v.get<float>();
            } else if (v.is_object()) {
                info.Fps = Number(v.value("fps", Json(8.0f)), 8);
                info.Loop = v.value("loop", true);
                if (v.contains("durations") && v["durations"].is_array())
                    for (auto& ticks : v["durations"])
                        info.Ticks.push_back(Max(1, ticks.is_number() ? ticks.get<int>() : 1));
                if (v.contains("events") && v["events"].is_object())
                    for (auto& [event, frames] : v["events"].items())
                        if (frames.is_array())
                            for (auto& frame : frames)
                                if (frame.is_number_integer()) info.Events.emplace_back(event, frame.get<int>());
            }
            data->Animations[name] = std::move(info);
        }

    // "sockets": {"sprite": {"hand": [x, y, angleDegrees?]}}, offsets in sprite pixels.
    if (json.contains("sockets") && json["sockets"].is_object())
        for (auto& [sprite, list] : json["sockets"].items()) {
            auto id = data->Ids.find(sprite);
            if (id == data->Ids.end() || !list.is_object()) continue;
            float density = data->Frames[std::size_t(id->second)].Density;
            for (auto& [name, v] : list.items())
                if (v.is_array() && v.size() >= 2)
                    data->Sockets[id->second][name] =
                        BitMotor::Socket{Vec2(Number(v[0]), Number(v[1])) * (1.0f / density),
                                         (v.size() > 2 ? Number(v[2]) : 0.0f) * Pi / 180.0f};
        }

    Atlas atlas;
    atlas.Data = std::move(data);
    return atlas;
}

Atlas Atlas::Load(const std::filesystem::path& json) {
    std::filesystem::path full = json.is_relative() ? Files::AssetPath(json) : json;
    auto text = Files::Read(full);
    if (!text) {
        Log::Error("Cannot read the atlas {}", full.string());
        return {};
    }
    // nyiengisprite writes "filter": "nearest" for pixel art; hand-made atlases of smooth art say "linear".
    Json peek = Json::parse(*text, nullptr, false);
    Filter filter = Filter::Nearest;
    if (peek.is_object() && peek.value("filter", std::string("nearest")) == "linear") filter = Filter::Linear;
    std::filesystem::path imagePath = full;
    imagePath.replace_extension(".png");
    Texture image = Texture::Load(imagePath, filter);
    if (!image) return {};
    Atlas atlas = Parse(*text, image, full.filename().string());
    if (atlas) Log::Info("Atlas {}: {} sprites", full.filename().string(), atlas.Data->Frames.size());
    return atlas;
}

Region Atlas::Sprite(std::string_view name) const {
    if (!Data) return {};
    auto id = Data->Ids.find(std::string(name));
    if (id == Data->Ids.end()) {
        Data->WarnOnce("sprite", name);
        return {};
    }
    return Data->MakeRegion(id->second);
}

bool Atlas::Has(std::string_view name) const { return Data && Data->Ids.count(std::string(name)) != 0; }

BitMotor::Animation Atlas::Animation(std::string_view name) const {
    BitMotor::Animation animation;
    if (!Data) return animation;
    std::string key(name);
    if (auto first = Data->Ids.find(key + "_0"); first != Data->Ids.end()) {
        animation.First = first->second;
        while (Data->Ids.count(key + "_" + std::to_string(animation.FrameCount))) ++animation.FrameCount;
    } else if (auto single = Data->Ids.find(key); single != Data->Ids.end()) {
        animation.First = single->second;
        animation.FrameCount = 1;
    } else {
        Data->WarnOnce("animation", name);
        return animation;
    }
    if (auto info = Data->Animations.find(key); info != Data->Animations.end()) {
        animation.FramesPerSecond = info->second.Fps;
        animation.Looping = info->second.Loop;
        animation.Ticks = info->second.Ticks;
        animation.Events = info->second.Events;
    }
    animation.Data = Data;
    return animation;
}

std::optional<BitMotor::Socket> Atlas::Socket(const Region& frame, std::string_view name) const {
    if (!Data || frame.Id < 0 || !(frame.Source == Data->Image)) return std::nullopt;
    auto sockets = Data->Sockets.find(frame.Id);
    if (sockets == Data->Sockets.end()) return std::nullopt;
    auto socket = sockets->second.find(name);
    if (socket == sockets->second.end()) return std::nullopt;
    return socket->second;
}

std::vector<std::string> Atlas::Names() const {
    std::vector<std::string> names;
    if (!Data) return names;
    for (auto& [name, id] : Data->Ids) names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

Texture Atlas::Image() const { return Data ? Data->Image : Texture(); }

// ------------------------------------------------------------------------------------------ Animation

namespace {

int TotalTicks(const std::vector<int>& ticks, int count) {
    int total = 0;
    for (int i = 0; i < count; ++i) total += TicksOf(ticks, i);
    return Max(1, total);
}

} // namespace

int Animation::FrameIndex(float seconds, bool loop) const {
    if (FrameCount <= 0) return 0;
    int total = TotalTicks(Ticks, FrameCount);
    int tick = FramesPerSecond > 0 ? int(std::floor(Max(0.0f, seconds) * FramesPerSecond)) : 0;
    tick = loop ? tick % total : Min(tick, total - 1);
    int frame = 0;
    while (frame < FrameCount - 1 && tick >= TicksOf(Ticks, frame)) tick -= TicksOf(Ticks, frame++);
    return frame;
}

int Animation::FrameIndex(float seconds) const { return FrameIndex(seconds, Looping); }

Region Animation::Frame(float seconds, bool loop) const {
    if (!Data || FrameCount <= 0) return {};
    return Data->MakeRegion(First + FrameIndex(seconds, loop));
}

Region Animation::Frame(float seconds) const { return Frame(seconds, Looping); }

float Animation::Duration() const {
    return FrameCount <= 0 || FramesPerSecond <= 0 ? 0.0f : float(TotalTicks(Ticks, FrameCount)) / FramesPerSecond;
}

bool Animation::Done(float seconds) const { return !Looping && seconds >= Duration(); }

bool Animation::Fired(std::string_view event, float from, float to) const {
    if (FrameCount <= 0 || FramesPerSecond <= 0 || to <= from) return false;
    float period = Duration();
    for (auto& [name, frame] : Events) {
        if (name != event || frame < 0 || frame >= FrameCount) continue;
        int start = 0;
        for (int i = 0; i < frame; ++i) start += TicksOf(Ticks, i);
        float at = float(start) / FramesPerSecond;
        if (!Looping) {
            if (from <= at && at < to) return true;
            continue;
        }
        // The first pass that starts at or after `from`, then one per period.
        float passes = std::ceil((from - at) / period);
        for (float when = at + Max(0.0f, passes) * period; when < to; when += period)
            if (when >= from) return true;
    }
    return false;
}

} // namespace BitMotor
