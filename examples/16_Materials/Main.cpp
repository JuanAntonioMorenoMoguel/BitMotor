// 16_Materials: physically based materials (metal/roughness, as glTF) for realistic models.
// - Two rows of spheres: plastic (top) and gold (bottom), from glossy on the left to rough on the right.
// - A tiled floor whose colour, normal and roughness maps are made in code: the grout is sunk and matte, the
//   tiles are bevelled and shiny. The normal map needs no tangents in the mesh.
// - A warm point light circles the spheres; the scene goes through the filmic (ACES) tone curve with
//   Config::LinearColor, the way realistic 3D wants it.
// Right drag orbits, the wheel zooms. P: physical materials on/off (off: the plain lighting of before).
// T: filmic or soft tone curve. Up/Down: exposure. Esc quits.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <cstdint>
#include <format>
#include <vector>

using namespace BitMotor;

namespace {

constexpr int TileSize = 256, Tiles = 4, Bevel = 7, Grout = 3;

// 0 in the grout, rising over the bevel to 1 on the tile.
float Height(int x, int y) {
    const int cell = TileSize / Tiles;
    const int px = ((x % TileSize) + TileSize) % TileSize % cell, py = ((y % TileSize) + TileSize) % TileSize % cell;
    const int edge = std::min({px, py, cell - 1 - px, cell - 1 - py}) - Grout;
    return edge < 0 ? 0.0f : std::min(1.0f, float(edge + 1) / float(Bevel));
}

std::uint8_t Byte(float v) { return std::uint8_t(std::clamp(v, 0.0f, 1.0f) * 255 + 0.5f); }

struct FloorMaps {
    Texture Colour, Normal, MetalRough;
};

FloorMaps MakeFloor() {
    std::vector<std::uint8_t> colour, normal, metalRough;
    const int cell = TileSize / Tiles;
    for (int y = 0; y < TileSize; ++y)
        for (int x = 0; x < TileSize; ++x) {
            const float h = Height(x, y);
            // Each tile a slightly different shade (a fixed pattern, no random numbers).
            const int tile = (x / cell) * 7 + (y / cell) * 13;
            const float shade = 0.85f + 0.15f * float(tile % 5) / 4.0f;
            const Color tileColour{0.52f * shade, 0.47f * shade, 0.42f * shade, 1};
            const Color c = h > 0 ? tileColour : Color{0.16f, 0.15f, 0.14f, 1};
            colour.insert(colour.end(), {Byte(c.R), Byte(c.G), Byte(c.B), 255});
            // Slopes of the height: x to the right, green up (rows grow downwards).
            const float dx = (Height(x + 1, y) - Height(x - 1, y)) * 1.5f;
            const float dy = (Height(x, y + 1) - Height(x, y - 1)) * 1.5f;
            const Vec3 n = Normalize(Vec3{-dx, dy, 1});
            normal.insert(normal.end(), {Byte(n.X * 0.5f + 0.5f), Byte(n.Y * 0.5f + 0.5f), Byte(n.Z * 0.5f + 0.5f), 255});
            metalRough.insert(metalRough.end(), {0, Byte(h > 0 ? 0.3f : 0.95f), 0, 255});
        }
    return {Texture::FromPixels(TileSize, TileSize, colour, Filter::Linear),
            Texture::FromPixels(TileSize, TileSize, normal, Filter::Linear),
            Texture::FromPixels(TileSize, TileSize, metalRough, Filter::Linear)};
}

// A plane whose UVs repeat `repeat` times.
Mesh TiledPlane(float size, float repeat) {
    std::vector<Vertex3D> v = Mesh::Plane({size, size}).Vertices();
    for (Vertex3D& p : v) p.Uv = p.Uv * repeat;
    return Mesh::Create(v, Mesh::Plane({size, size}).Indices());
}

} // namespace

struct MaterialsExample : Game {
    std::vector<Material> looks;
    Material floorLook;
    Mesh sphere, floorMesh;
    std::vector<Model> spheres;
    Model floor, lamp;
    FloorMaps maps;
    float yaw = 0.35f, pitch = 0.32f, distance = 7, time = 0;
    bool physical = true;

    void Build() {
        spheres.clear();
        for (Material m : looks) {
            if (!physical) m.Metallic = 0, m.Roughness = 1;
            spheres.push_back(Model::FromMesh(sphere, m));
        }
        Material f = floorLook;
        if (!physical) f.NormalMap = {}, f.MetalRoughMap = {}, f.Roughness = 1;
        floor = Model::FromMesh(floorMesh, f);
    }

    void Start() override {
        sphere = Mesh::Sphere(0.42f, 48);
        floorMesh = TiledPlane(12, 6);
        maps = MakeFloor();
        for (int row = 0; row < 2; ++row)
            for (int i = 0; i < 5; ++i) {
                Material m;
                m.Tint = row == 0 ? Color::Hex(0xC23B30) : Color::Hex(0xF2C14E);
                m.Metallic = row == 0 ? 0.0f : 1.0f;
                m.Roughness = 0.08f + 0.23f * float(i);
                looks.push_back(m);
            }
        floorLook = {.Image = maps.Colour, .Roughness = 1, .NormalMap = maps.Normal, .MetalRoughMap = maps.MetalRough};
        lamp = Model::FromMesh(Mesh::Sphere(0.08f, 12), {.Tint = Color::Hex(0xFFC890), .Emission = 3, .Unlit = true});
        Build();
        Lighting::SetToneMap(ToneMap::Filmic);
        Lighting::SetExposure(1.0f);
        Lighting::SetBloom(0.25f, 1.6f);
        Lighting3D::SetSun({-0.4f, -1, -0.6f}, Color::Hex(0xFFF2E0), 1.6f);
        Lighting3D::SetAmbient(Color::Hex(0x3A4252));
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        time += Time::Delta();
        distance = std::clamp(distance - Input::MouseWheel() * 0.5f, 3.0f, 16.0f);
        if (Input::MouseDown(MouseButton::Right)) {
            yaw -= Input::MouseDelta().X * 0.01f;
            pitch = std::clamp(pitch + Input::MouseDelta().Y * 0.01f, 0.05f, 1.4f);
        }
        if (Input::KeyPressed(Key::P)) {
            physical = !physical;
            Build();
        }
        if (Input::KeyPressed(Key::T))
            Lighting::SetToneMap(Lighting::Tone() == ToneMap::Filmic ? ToneMap::Soft : ToneMap::Filmic);
        if (Input::KeyPressed(Key::Up)) Lighting::SetExposure(Lighting::Exposure() * 1.25f);
        if (Input::KeyPressed(Key::Down)) Lighting::SetExposure(Lighting::Exposure() / 1.25f);
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x1A1E28));
        const Vec3 target{0, 0.9f, 0};
        const Vec3 offset{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
        Screen::SetCamera3D({.Position = target + offset * distance, .Target = target, .Fov = 45});
        Screen::Draw(Model3D{.Source = floor});
        for (std::size_t k = 0; k < spheres.size(); ++k) {
            const float x = (float(k % 5) - 2) * 1.05f, y = k < 5 ? 1.55f : 0.5f;
            Screen::Draw(Model3D{.Source = spheres[k], .Position = {x, y, 0}});
        }
        const Vec3 light{std::cos(time * 0.7f) * 3.2f, 1.4f, std::sin(time * 0.7f) * 1.8f + 1.0f};
        Screen::Draw(PointLight3D{.Position = light, .Radius = 5, .Color = Color::Hex(0xFFC890), .Intensity = 2.5f});
        Screen::Draw(Model3D{.Source = lamp, .Position = light});
        Text::Draw(std::format("P: physical {}   T: {} tone   Up/Down: exposure {:.2f}   right drag: orbit",
                               physical ? "on" : "off", Lighting::Tone() == ToneMap::Filmic ? "filmic" : "soft",
                               Lighting::Exposure()),
                   {8, 8}, {.Shadow = Colors::Black});
    }
};

int main() {
    return Run<MaterialsExample>({.Title = "BitMotor - 16 Materials",
                                  .Width = 960,
                                  .Height = 540,
                                  .IntegerScaling = false,
                                  .LinearColor = true,
                                  .TickRate = 0});
}
