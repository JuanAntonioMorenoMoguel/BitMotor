#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Matrix.h"
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Texture.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Simple 3D: low-poly models with colours per vertex or a texture, lit by a sun, an ambient light and point
// lights, drawn under the 2D World layer (sprites, text and UI go on top). Right-handed, +Y up, 1 unit = 1
// metre; a model looks down -Z.
//     Model robot = Model::Load("models/robot.bitmodel");               // made with BitModel
//     Mesh floor = Mesh::Plane({20, 20});
//     Screen::SetCamera3D({.Position = {0, 4, 8}, .Target = {0, 1, 0}});
//     Screen::Draw(Model3D{.Source = Model::FromMesh(floor), .Tint = Colors::Slate});
//     Screen::Draw(Model3D{.Source = robot, .Position = spot, .Rotation = facing,
//                          .Animation = "walk", .Time = walkTime});
//     Screen::Draw(PointLight3D{.Position = torch, .Radius = 6, .Color = Colors::Amber});
//     if (robot.Animation("walk").Fired("step", lastTime, walkTime)) PlayStep();
// The format of .bitmodel files is in docs/BitModel.md.
namespace BitMotor {

struct Vertex3D {
    Vec3 Position = {};
    Vec3 Normal = {}; // all zero in every vertex: Mesh::Create makes flat normals
    Vec2 Uv = {};     // (0, 0) at the top left of the texture
    Color Tint = Colors::White;
};
static_assert(sizeof(Vertex3D) == 48);

struct Material {
    // Multiplies the texture and the vertex colours.
    Color Tint = Colors::White;
    Texture Image = {};
    // 0: lit as usual. Adds its colour times 1.5 x Emission on top of the light, so it glows in the dark
    // (and blooms above 1).
    float Emission = 0;
    // Ignores the light: shown with its own colour.
    bool Unlit = false;
};

struct ModelFile;

namespace Detail {
struct MeshData;
struct ModelData;
} // namespace Detail

// Triangles on the GPU (uploaded the first time they are drawn).
class Mesh {
public:
    Mesh() = default;
    // Three indices per triangle, counter-clockwise seen from outside. No indices: the vertices three by
    // three. Vertices whose normals are all zero get flat normals (one per face).
    static Mesh Create(std::vector<Vertex3D> vertices, std::vector<std::uint32_t> indices = {});
    // Centred on the origin.
    static Mesh Cube(Vec3 size = {1, 1, 1});
    static Mesh Sphere(float radius = 0.5f, int segments = 16);
    // Lying flat (facing +Y).
    static Mesh Plane(Vec2 size = {1, 1});
    // Standing on Y, from -height/2 to +height/2.
    static Mesh Cylinder(float radius = 0.5f, float height = 1, int segments = 16);

    const std::vector<Vertex3D>& Vertices() const;
    const std::vector<std::uint32_t>& Indices() const;
    // The box around it: lowest and highest corner.
    Vec3 Min() const;
    Vec3 Max() const;
    explicit operator bool() const { return Data != nullptr; }
    const std::shared_ptr<Detail::MeshData>& Internal() const { return Data; }

private:
    std::shared_ptr<Detail::MeshData> Data;
};

// One animation of a model, as authored: it keeps no time of its own (pass it the time in Model3D).
class ModelAnimation {
public:
    ModelAnimation() = default;
    float Duration() const { return Seconds; }
    bool Loops() const { return Looping; }
    // True if `event` happens between `from` (included) and `to` (excluded): call it each Update with the
    // previous and the current time so it fires exactly once per pass.
    bool Fired(std::string_view event, float from, float to) const;
    explicit operator bool() const { return Valid; }

private:
    friend class Model;
    float Seconds = 0;
    bool Looping = true;
    bool Valid = false;
    std::vector<std::pair<std::string, float>> Events;
};

// Meshes with materials in a tree of nodes, their animations and sockets: a .bitmodel file.
class Model {
public:
    Model() = default;
    // A .bitmodel (relative paths: Files::AssetPath). Empty (and a log error) if it cannot be read.
    static Model Load(const std::filesystem::path& path);
    // From the text of a .bitmodel; textures are looked for in `folder`. `name` is for the log.
    static Model Parse(std::string_view json, const std::filesystem::path& folder = {},
                       std::string_view name = "model");
    // A ModelFile (ModelFile.h) ready to draw; textures are looked for in `folder`. Names that point nowhere
    // are warned about and left out.
    static Model FromFile(const ModelFile& file, const std::filesystem::path& folder = {},
                          std::string_view name = "model");
    // One mesh as a model with a single node.
    static Model FromMesh(const Mesh& mesh, const Material& material = {});

    ModelAnimation Animation(std::string_view name) const;
    std::vector<std::string> Animations() const;
    std::vector<std::string> Nodes() const;
    // Where every node is (in the model's space) at `seconds` into `animation` (empty: the rest pose).
    std::vector<Mat4> Pose(std::string_view animation = {}, float seconds = 0) const;
    // Where a socket is, in the model's space, for that animation and time. nullopt if it does not exist.
    std::optional<Mat4> Socket(std::string_view name, std::string_view animation = {}, float seconds = 0) const;

    explicit operator bool() const { return Data != nullptr; }
    const std::shared_ptr<Detail::ModelData>& Internal() const { return Data; }

private:
    std::shared_ptr<Detail::ModelData> Data;
};

// A model to draw this frame.
struct Model3D {
    BitMotor::Model Source;
    Vec3 Position = {0, 0, 0};
    Quat Rotation = {};
    Vec3 Scale = {1, 1, 1};
    // Multiplies every material's colour.
    Color Tint = Colors::White;
    // The animation and how far into it (seconds). Empty: the rest pose.
    std::string_view Animation = {};
    float Time = 0;
};

struct Camera3D {
    Vec3 Position = {0, 2, 6};
    Vec3 Target = {0, 0, 0};
    Vec3 Up = {0, 1, 0};
    // Degrees from the top of the screen to the bottom.
    float Fov = 60;
    float Near = 0.1f;
    float Far = 200;
    // Orthographic: no perspective, `Height` units fit from top to bottom.
    bool Orthographic = false;
    float Height = 10;
    // Where on the logical screen the 3D is drawn (an editor's 3D panel). Empty: the whole screen. One camera
    // per frame, so one 3D view.
    Rect Viewport = {};

    // The viewport, or the whole logical screen when it is empty.
    Rect Area() const;
    Mat4 View() const;
    Mat4 Projection(float aspect) const;
    // Projection * View for the shape of Area().
    Mat4 ViewProjection() const;
    // Where a point of the world is on the logical screen (pixels, inside Area()). nullopt if it is behind
    // the camera.
    std::optional<Vec2> ToScreen(Vec3 world) const;
    // The direction from the camera through a point of the logical screen (for picking).
    Vec3 Ray(Vec2 screen) const;
    // Where the ray through `screen` starts: the camera for a perspective one; on the near plane, under that
    // point, for an orthographic one.
    Vec3 RayOrigin(Vec2 screen) const;
};

struct PointLight3D {
    Vec3 Position = {0, 2, 0};
    // Reach in units: full at the centre, nothing at the edge.
    float Radius = 5;
    BitMotor::Color Color = Colors::White;
    float Intensity = 1;
};

// A line in the 3D world, `Width` logical pixels wide whatever its distance: grids, axes, wireframes, gizmos,
// debugging. Hidden behind models unless `OnTop`.
struct Line3D {
    Vec3 From = {0, 0, 0};
    Vec3 To = {0, 0, 0};
    BitMotor::Color Color = Colors::White;
    float Width = 1;
    bool OnTop = false;
};

// A square dot in the 3D world, `Size` logical pixels across (selected vertices, handles).
struct Point3D {
    Vec3 Position = {0, 0, 0};
    BitMotor::Color Color = Colors::White;
    float Size = 5;
    bool OnTop = false;
};

namespace Screen {
void SetCamera3D(const Camera3D& camera);
const Camera3D& CurrentCamera3D();
void Draw(const Model3D& model);
// Lights the models of this frame (up to 8 per model: the closest ones).
void Draw(const PointLight3D& light);
// Drawn with the models, unlit, with the camera of the end of the frame.
void Draw(const Line3D& line);
void Draw(const Point3D& point);
} // namespace Screen

// The light of the 3D world: a sun (a direction) and an ambient light. Lighting::SetStepped bands it too.
namespace Lighting3D {
// `direction`: where the sun's light travels (down and a little to the side by default).
void SetSun(Vec3 direction, Color color = Colors::White, float intensity = 1);
Vec3 SunDirection();
Color SunColor();
void SetAmbient(Color ambient);
Color Ambient();
} // namespace Lighting3D

} // namespace BitMotor
