#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Texture.h"

#include <optional>

// Drawing sprites. Call Screen::Draw during Draw (a scene's, a manager's or the game's); the frame is put
// together at the end, layer by layer.
//     Screen::Draw(Sprite2D{.Region = atlas.Sprite("tree"), .Position = {120, 80}, .Depth = 80});
//     Screen::Draw(Sprite2D{.Region = heart, .Position = {4, 4}, .Layer = Layer::Hud});
namespace BitMotor {

// From back to front. Background, World, Glow and Overlay move with the camera; Hud stays on the screen.
enum class Layer {
    Background, // skies and far scenery, behind everything
    World,      // the game itself, sorted by Depth (the y of the feet makes things walk in front or behind)
    Glow,       // added on top of the world (fire, magic, sparks): it brightens instead of covering
    Overlay,    // on top of the world but still in it (health bars, damage numbers)
    Hud,        // the interface, fixed on the screen
};

struct Sprite2D {
    BitMotor::Region Region = {};
    // Where the region's pivot goes: world pixels, or screen pixels in the Hud layer.
    Vec2 Position = {0, 0};
    // Radians, clockwise on the screen, around the pivot.
    float Rotation = 0;
    Vec2 Scale = {1, 1};
    // Overrides the region's pivot, as a fraction of its size: {0.5f, 0.5f} is the centre.
    std::optional<Vec2> Origin = std::nullopt;
    // Multiplies the texture's colour (and alpha). White leaves it as it is.
    BitMotor::Color Color = Colors::White;
    // Mirrors it left to right (the pivot mirrors with it).
    bool Flip = false;
    // 0..1 towards plain white: the classic hit flash.
    float Flash = 0;
    BitMotor::Layer Layer = Layer::World;
    // Order inside the World layer, from low (behind) to high (in front). Equal depths keep the draw order.
    float Depth = 0;
};

namespace Screen {
// Queues the sprite for this frame. An empty Region draws nothing.
void Draw(const Sprite2D& sprite);
} // namespace Screen

} // namespace BitMotor
