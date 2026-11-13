#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Vector.h"

// 2D lighting and screen effects. Lighting is off until the game turns it on; then the World layer is lit
// by the ambient colour and the lights drawn this frame, following the normal maps (atlas_n.png next to
// atlas.png). Background, Glow, Overlay and Hud are never lit.
//     Lighting::Enable(true);
//     Lighting::SetAmbient(Color::Hex(0x202840));
//     Lighting::SetBloom(0.6f);
//     Screen::Draw(Light2D{.Position = torch, .Radius = 90, .Color = Colors::Amber});   // each frame
//     Screen::SetEffects({.Vignette = 0.35f, .Grain = 0.03f});
//     Screen::Flash(Colors::White, 0.2f);                                              // a hit
namespace BitMotor {

struct Light2D {
    // World pixels (moves with the camera).
    Vec2 Position = {0, 0};
    // Height above the floor, in pixels: low lights graze the normal maps, high ones light them evenly.
    float Height = 24;
    // Reach in pixels: full at the centre, nothing at the edge.
    float Radius = 64;
    BitMotor::Color Color = Colors::White;
    float Intensity = 1;
};

// The screen effects, applied to everything below the Hud. All off by default.
struct ScreenEffects {
    // Darkens the corners (0..1).
    float Vignette = 0;
    // Film grain (0..0.1 is plenty).
    float Grain = 0;
    // Colour fringes towards the edges, in pixels.
    float Aberration = 0;
    // 0 grey, 1 as drawn, above 1 more saturated.
    float Saturation = 1;
};

namespace Screen {
// Lights this frame's World layer (only while Lighting is on).
void Draw(const Light2D& light);

void SetEffects(const ScreenEffects& effects);
const ScreenEffects& Effects();

// Covers the screen (below the Hud) with `color`, fading out in `seconds`. Does nothing when the player
// turned flashes off (Settings::Flashes).
void Flash(Color color = Colors::White, float seconds = 0.15f);
} // namespace Screen

// How the HDR scene becomes screen colours. Soft: linear up to 0.8 and a gentle shoulder, only while Lighting
// is on (keeps a palette as it is). Filmic: the ACES curve, always, for realistic 3D (with Config::LinearColor).
enum class ToneMap { Soft, Filmic };

namespace Lighting {
void Enable(bool on);
bool Enabled();
// The light everything gets without any lamp. Dark blue-grey by default.
void SetAmbient(Color ambient);
Color Ambient();
// Bands of light with ordered dithering instead of smooth gradients (suits pixel art).
void SetStepped(bool stepped);
bool Stepped();
// Glow around bright pixels: 0 turns it off. `threshold`: how bright a pixel must be to glow.
// Works with lighting on or off.
void SetBloom(float strength, float threshold = 0.8f);
float Bloom();
float BloomThreshold();
void SetToneMap(ToneMap tone);
ToneMap Tone();
// Multiplies the whole scene before the tone curve (1: as it is; 2: one stop brighter).
void SetExposure(float exposure);
float Exposure();
} // namespace Lighting

} // namespace BitMotor
