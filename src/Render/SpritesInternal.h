#pragma once
#include "BitMotor/Render/Camera.h"
#include "BitMotor/Render/Sprite.h"
#include "BitMotor/Render/Texture.h"

#include <cstdint>
#include <vector>

// The sprites of one frame, on the CPU: Draw queues them already in screen pixels (the camera is applied
// at that moment), and the renderer sorts them into one instance list with one batch per texture run.
namespace BitMotor::Detail {

// What the sprite shader reads for each sprite (std430, 64 bytes; see Shaders/Sprite.vert).
struct SpriteInstance {
    float X = 0, Y = 0, Width = 0, Height = 0;             // pivot in screen pixels, size (Width < 0: flipped)
    float U = 0, V = 0, UWidth = 0, VHeight = 0;           // area in texture pixels
    float R = 1, G = 1, B = 1, A = 1;                      // colour
    float Rotation = 0, Flash = 0, PivotX = 0, PivotY = 0; // pivot as a fraction of the size
};
static_assert(sizeof(SpriteInstance) == 64);

struct QueuedSprite {
    SpriteInstance Instance;
    Texture Source; // keeps the texture alive until the frame is drawn
    Layer Where = Layer::World;
    float Depth = 0;
};

// A run of instances with the same texture and layer: one draw call.
struct SpriteBatch {
    Texture Source;
    Layer Where = Layer::World;
    std::uint32_t First = 0, Count = 0;
};

// The sprite as it goes to the GPU, seen through `camera` (nullptr: screen pixels, for the Hud).
SpriteInstance MakeInstance(const Sprite2D& sprite, const Camera2D* camera);

// This frame's queue (Draw adds to it, the renderer empties it).
std::vector<QueuedSprite>& SpriteQueue();
// Back to the default camera and an empty queue, at the start of each frame.
void ResetSprites();

// Orders the queue by layer (World by depth, keeping the draw order on ties) and groups it into batches.
void BuildSprites(const std::vector<QueuedSprite>& queue, std::vector<SpriteInstance>& instances,
                  std::vector<SpriteBatch>& batches);

} // namespace BitMotor::Detail
