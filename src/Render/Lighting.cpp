#include "BitMotor/Core/Settings.h"
#include "BitMotor/Math/Functions.h"
#include "Render/SpritesInternal.h"

namespace BitMotor {

namespace {

bool LightingOn = false;
Color AmbientLight = Color::Hex(0x3A4055);
bool SteppedLight = false;
float BloomStrength = 0, BloomLevel = 0.8f;
ScreenEffects CurrentEffects;
std::vector<Detail::LightInstance> Lights;

Color FlashTint = Colors::White;
float FlashLeft = 0, FlashLength = 0;

} // namespace

namespace Screen {

void Draw(const Light2D& light) {
    if (light.Radius <= 0 || light.Intensity <= 0) return;
    Lights.push_back(Detail::MakeLight(light, CurrentCamera()));
}

void SetEffects(const ScreenEffects& effects) { CurrentEffects = effects; }
const ScreenEffects& Effects() { return CurrentEffects; }

void Flash(Color color, float seconds) {
    if (!Settings::Get().Flashes || seconds <= 0) return;
    FlashTint = color;
    FlashLength = seconds;
    FlashLeft = seconds;
}

} // namespace Screen

namespace Lighting {
void Enable(bool on) { LightingOn = on; }
bool Enabled() { return LightingOn; }
void SetAmbient(Color ambient) { AmbientLight = ambient; }
Color Ambient() { return AmbientLight; }
void SetStepped(bool stepped) { SteppedLight = stepped; }
bool Stepped() { return SteppedLight; }
void SetBloom(float strength, float threshold) {
    BloomStrength = Max(strength, 0.0f);
    BloomLevel = threshold;
}
float Bloom() { return BloomStrength; }
float BloomThreshold() { return BloomLevel; }
} // namespace Lighting

namespace Detail {

LightInstance MakeLight(const Light2D& light, const Camera2D& camera) {
    LightInstance l;
    Vec2 at = camera.ToScreen(light.Position);
    l.X = at.X;
    l.Y = at.Y;
    l.Height = light.Height * camera.Zoom;
    l.Radius = light.Radius * camera.Zoom;
    l.R = light.Color.R * light.Intensity;
    l.G = light.Color.G * light.Intensity;
    l.B = light.Color.B * light.Intensity;
    return l;
}

std::vector<LightInstance>& LightQueue() { return Lights; }

Color FlashColor() { return FlashTint; }
float FlashAmount() { return FlashLength > 0 ? Clamp(FlashLeft / FlashLength, 0.0f, 1.0f) * FlashTint.A : 0.0f; }
void AdvanceFlash(float seconds) {
    FlashLeft = Max(0.0f, FlashLeft - seconds);
    if (FlashLeft <= 0) FlashLength = 0;
}

} // namespace Detail

} // namespace BitMotor
