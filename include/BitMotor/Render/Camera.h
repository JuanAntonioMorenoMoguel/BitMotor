#pragma once
#include "BitMotor/Math/Vector.h"

// What part of the world the screen shows. The game keeps its cameras and says which one to use each frame.
//     Camera2D camera;                       // Position is the world point at the centre of the screen
//     camera.Position = Damp(camera.Position, player, 8, Time::Delta());
//     camera.AddShake(0.5f);                 // an explosion
//     camera.Update();                       // in Update: the shake fades out
//     SetCamera(camera);                     // in Draw, before drawing the world
namespace BitMotor {

struct Camera2D {
    // World point at the centre of the screen.
    Vec2 Position = {0, 0};
    // 2 shows everything twice as big.
    float Zoom = 1;
    // Radians: turns the view (the world appears turned the other way).
    float Rotation = 0;

    // Shake: Trauma (0..1) grows with AddShake and fades by ShakeDecay per second in Update. The shake is
    // Trauma squared, so small hits barely move and big ones shake hard. Scaled by Settings::ScreenShake.
    float Trauma = 0;
    float ShakeDecay = 1.5f;
    // Strongest shake, at Trauma 1: pixels and radians.
    float MaxShake = 8;
    float MaxShakeAngle = 0.05f;
    // How fast it trembles (noise steps per second).
    float ShakeSpeed = 25;

    void AddShake(float amount);
    // Call once per Update: fades the trauma and moves the shake along.
    void Update();
    // The current shake, already scaled by the player's setting.
    Vec2 ShakeOffset() const;
    float ShakeAngle() const;

    // Screen pixels (logical) <-> world pixels, with this camera (shake included).
    Vec2 ToWorld(Vec2 screen) const;
    Vec2 ToScreen(Vec2 world) const;

    // Internal: shake time, moved by Update.
    float ShakeTime = 0;
};

// The camera for the rest of this frame's drawing. Each frame starts with a camera at the centre of the
// screen (world pixels = screen pixels).
void SetCamera(const Camera2D& camera);
// The camera in use now.
const Camera2D& CurrentCamera();

} // namespace BitMotor
