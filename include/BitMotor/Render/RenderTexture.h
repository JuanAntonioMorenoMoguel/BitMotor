#pragma once
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Render/Texture.h"

// A texture the game draws into, then uses like any other: minimaps, portraits, a menu drawn once and
// kept, a mirror. Between Begin and End, Screen::Draw, Text and the UI's shapes go into it instead of the
// screen, in its own pixels (0,0 at its top-left; the camera starts centred on it, so the World layer
// lines up too). It is drawn at the end of the frame, before the screen, so using it in the same frame
// shows what was just drawn; a frame with no Begin keeps the last image.
//     RenderTexture minimap = RenderTexture::Create(64, 64);
//     // Draw:
//     minimap.Begin(Colors::Black);
//     for (const Room& room : rooms) UI::Rect(room.Area * 0.1f, Colors::Gray);
//     minimap.End();
//     Screen::Draw(Sprite2D{.Region = minimap.Full(), .Position = {250, 10}, .Layer = Layer::Hud});
// Everything in it is drawn plainly, in the order of the layers: no lights, normal maps, bloom, screen
// effects or 3D models, and the UI's widgets do not react inside it (they read the mouse on the screen).
// Pixels that are only partly opaque over a transparent clear come out a little darker: clear it with an
// opaque colour when that matters.
namespace BitMotor {

class RenderTexture {
public:
    RenderTexture() = default;

    // An empty (transparent) texture of that size. It can be made before the window opens.
    static RenderTexture Create(int width, int height, Filter filter = Filter::Nearest);

    // From now on, drawing goes into it, over `clear`. One at a time.
    void Begin(Color clear = Colors::Transparent);
    // The same, drawing over what it already has.
    void Continue();
    // Back to the screen.
    void End();

    // The texture, for a Sprite2D or a Region.
    const BitMotor::Texture& Source() const { return Image; }
    Region Full(Vec2 pivot = {0, 0}) const { return Image.Full(pivot); }
    int Width() const { return Image.Width(); }
    int Height() const { return Image.Height(); }
    Vec2 Size() const { return Image.Size(); }

    explicit operator bool() const { return bool(Image); }

private:
    BitMotor::Texture Image;
};

} // namespace BitMotor
