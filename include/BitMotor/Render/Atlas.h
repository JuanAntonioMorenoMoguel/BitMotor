#pragma once
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Texture.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A sprite sheet in nyiengisprite's format: atlas.json next to its image (atlas.png).
//     Atlas atlas = Atlas::Load("atlas.json");                 // relative paths: Files::AssetPath
//     Region coin = atlas.Sprite("coin");
//     Animation walk = atlas.Animation("hero_walk");           // hero_walk_0, hero_walk_1...
//     Screen::Draw(Sprite2D{.Region = walk.Frame(time), .Position = feet});
//     if (walk.Fired("step", lastTime, time)) PlayStep();     // events marked in the .sprite file
//     if (auto hand = atlas.Socket(walk.Frame(time), "hand")) DrawSword(feet + hand->Offset);
//
// atlas.json: {"filter": "nearest" | "linear", "sprites": {"name": [x, y, w, h, pivotX, pivotY, density?]},
//              "animations": {"name": fps | {"fps", "loop", "durations": [ticks...], "events": {"hit": [frame]}}},
//              "sockets": {"sprite": {"hand": [x, y, angle?]}}}
namespace BitMotor {

// Attachment point of one frame: where a weapon goes, where an arrow comes out.
struct Socket {
    // From the pivot, in screen pixels at Scale 1 (negate X when the sprite is drawn with Flip).
    Vec2 Offset = {0, 0};
    // Direction of its bone in radians: 0 points right, Pi/2 down (with Flip: Pi - Angle).
    float Angle = 0;
};

namespace Detail {
struct AtlasData;
}

// Frames of an animation and how they play, as authored: Frame(time) picks the one for `time` seconds
// since it started. It keeps no time of its own, so one Animation serves every entity that uses it.
class Animation {
public:
    Animation() = default;

    // The frame `seconds` after the start, looping or stopping on the last frame as authored.
    Region Frame(float seconds) const;
    Region Frame(float seconds, bool loop) const;
    // The frame number (0..Count-1) for that time.
    int FrameIndex(float seconds) const;
    int FrameIndex(float seconds, bool loop) const;
    // A one-shot animation ('once' in the .sprite) has finished. Looping ones never do.
    bool Done(float seconds) const;
    // Seconds of one pass.
    float Duration() const;
    // True if `event` starts between `from` (included) and `to` (excluded): call it each Update with the
    // previous and the current time, so it fires exactly once per pass.
    bool Fired(std::string_view event, float from, float to) const;

    int Count() const { return FrameCount; }
    float Fps() const { return FramesPerSecond; }
    bool Loops() const { return Looping; }
    explicit operator bool() const { return FrameCount > 0; }

private:
    friend class Atlas;
    std::shared_ptr<const Detail::AtlasData> Data;
    int First = 0, FrameCount = 0;
    float FramesPerSecond = 8;
    bool Looping = true;
    std::vector<int> Ticks; // per frame, in 1/fps steps
    std::vector<std::pair<std::string, int>> Events;
};

class Atlas {
public:
    Atlas() = default;

    // Reads the JSON and its image (same name, .png). An empty atlas (and a log error) if it fails.
    static Atlas Load(const std::filesystem::path& json);

    // A sprite by name. An unknown name warns once and gives an empty Region (draws nothing).
    Region Sprite(std::string_view name) const;
    bool Has(std::string_view name) const;
    // "name" gathers name_0, name_1... in order (or just "name" as one frame). Warns if there is none.
    BitMotor::Animation Animation(std::string_view name) const;
    // The socket `name` of a frame of this atlas; nullopt if that frame does not have it.
    std::optional<BitMotor::Socket> Socket(const Region& frame, std::string_view name) const;
    // Every sprite name, sorted.
    std::vector<std::string> Names() const;

    Texture Image() const;
    explicit operator bool() const { return Data != nullptr; }

    // From JSON text and an already loaded image (Load uses it; also handy for tests).
    static Atlas Parse(std::string_view json, Texture image, std::string_view source = "atlas");

private:
    std::shared_ptr<Detail::AtlasData> Data;
};

} // namespace BitMotor
