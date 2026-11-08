// 12_3D: simple 3D under the 2D layers. A robot made with the .bitmodel format (Assets/robot.bitmodel, what
// BitModel exports) walks in a circle on a floor of meshes made in code (Mesh::Plane, Cube, Sphere,
// Cylinder), lit by the sun, the ambient and three coloured point lights. Its eyes are emissive and bloom.
// The game runs with Config::LinearColor, so the lights mix as they would in the real world.
// - The "step" events of the walk animation flash a dot on the 2D layer; a sphere hangs from its "hand" socket.
// - Left/Right orbit the camera, Up/Down zoom. Space: banded (stepped) lighting. W: the robot waves.
// Esc quits.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <format>

using namespace BitMotor;

struct ThreeDExample : Game {
    Model robot, floor, pillar, ball, lamp, hanging;
    float time = 0, angle = 0.6f, distance = 9, stepFlash = 0;
    bool waving = false;

    void Start() override {
        robot = Model::Load("12_3D/Assets/robot.bitmodel");
        floor = Model::FromMesh(Mesh::Plane({18, 18}), {.Tint = Color::Hex(0x5A6070)});
        pillar = Model::FromMesh(Mesh::Cylinder(0.35f, 2.4f, 12), {.Tint = Color::Hex(0xC8B89A)});
        ball = Model::FromMesh(Mesh::Sphere(0.5f, 20), {.Tint = Color::Hex(0xE05A4A)});
        lamp = Model::FromMesh(Mesh::Sphere(0.12f, 8), {.Emission = 2, .Unlit = true});
        hanging = Model::FromMesh(Mesh::Cube({0.2f, 0.2f, 0.2f}), {.Tint = Colors::Gold});
        Lighting::SetBloom(0.6f, 0.9f);
        Lighting3D::SetSun({-0.5f, -1, -0.4f}, Color::Hex(0xFFF0D8), 0.55f);
        Lighting3D::SetAmbient(Color::Hex(0x343A50));
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        const float dt = Time::Delta();
        if (Input::KeyDown(Key::Left)) angle -= 1.5f * dt;
        if (Input::KeyDown(Key::Right)) angle += 1.5f * dt;
        if (Input::KeyDown(Key::Up)) distance = std::max(4.0f, distance - 6 * dt);
        if (Input::KeyDown(Key::Down)) distance = std::min(18.0f, distance + 6 * dt);
        if (Input::KeyPressed(Key::Space)) Lighting::SetStepped(!Lighting::Stepped());
        if (Input::KeyPressed(Key::W)) waving = !waving;
        const float before = time;
        time += dt;
        if (!waving && robot.Animation("walk").Fired("step", before, time)) stepFlash = 0.15f;
        stepFlash = std::max(0.0f, stepFlash - dt);
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x10121C));
        Screen::SetCamera3D({.Position = {std::sin(angle) * distance, distance * 0.55f, std::cos(angle) * distance},
                             .Target = {0, 1, 0}});
        Screen::Draw(Model3D{.Source = floor});
        for (int i = 0; i < 4; ++i) {
            const float a = Pi / 4 + float(i) * Pi / 2;
            Screen::Draw(Model3D{.Source = pillar, .Position = {std::cos(a) * 6, 1.2f, std::sin(a) * 6}});
        }
        Screen::Draw(Model3D{.Source = ball, .Position = {0, 0.5f, 0}});

        // The robot walks around the ball, facing where it goes (it looks down -Z).
        const float around = waving ? 0 : time * 0.5f;
        const Vec3 at{std::cos(around) * 3, 0, std::sin(around) * 3};
        const Quat facing = Quat::LookRotation({-std::sin(around), 0, std::cos(around)});
        const std::string_view animation = waving ? "wave" : "walk";
        Screen::Draw(
            Model3D{.Source = robot, .Position = at, .Rotation = facing, .Animation = animation, .Time = time});
        if (auto hand = robot.Socket("hand", animation, time)) {
            const Mat4 placed = Mat4::Transform(at, facing) * *hand;
            Screen::Draw(Model3D{.Source = hanging, .Position = placed.TransformPoint(Vec3{0, -0.1f, 0})});
        }

        // Three lights circling, each with a small glowing bulb.
        const Color colors[3] = {Color::Hex(0xFF5040), Color::Hex(0x40FF90), Color::Hex(0x4080FF)};
        for (int i = 0; i < 3; ++i) {
            const float a = time * 0.7f + float(i) * 2 * Pi / 3;
            const Vec3 p{std::cos(a) * 5, 1.5f, std::sin(a) * 5};
            Screen::Draw(PointLight3D{.Position = p, .Radius = 6, .Color = colors[i], .Intensity = 1.6f});
            Screen::Draw(Model3D{.Source = lamp, .Position = p, .Tint = colors[i]});
        }

        Text::Draw(std::format("Arrows: camera  Space: bands ({})  W: {}", Lighting::Stepped() ? "on" : "off",
                               waving ? "walk" : "wave"),
                   {8, 8}, {.Shadow = Colors::Black});
        if (stepFlash > 0) UI::Circle({Screen::Width() - 16.0f, 16}, 5, Colors::Amber);
    }
};

int main() {
    return Run<ThreeDExample>({.Title = "BitMotor - 12 3D",
                               .Width = 640,
                               .Height = 360,
                               .IntegerScaling = false,
                               .LinearColor = true,
                               .TickRate = 0});
}
