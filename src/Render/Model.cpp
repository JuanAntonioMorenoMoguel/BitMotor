// 3D on the CPU side: meshes, .bitmodel files, poses and animations, the camera and the frame's queue.
#include "Render/ModelInternal.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Math/Functions.h"
#include "Core/FilesInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace BitMotor {

namespace Detail {

namespace {

std::vector<ModelDraw> Queue;
std::vector<PointLight3D> Lights;
Camera3D Camera;

Vec3 SunDirectionValue = Normalize(Vec3(-0.4f, -1.0f, -0.3f));
Color SunColorValue = Colors::White;
float SunIntensity = 0.9f;
Color AmbientValue{0.32f, 0.34f, 0.4f, 1};

void FinishMesh(MeshData& mesh) {
    // No indices: the vertices three by three.
    if (mesh.Indices.empty())
        for (std::uint32_t i = 0; i + 2 < mesh.Vertices.size(); i += 3)
            mesh.Indices.insert(mesh.Indices.end(), {i, i + 1, i + 2});
    std::erase_if(mesh.Indices, [&](std::uint32_t i) { return i >= mesh.Vertices.size(); });
    mesh.Indices.resize(mesh.Indices.size() / 3 * 3);
    // No normals at all: flat ones, so every triangle gets its own vertices.
    const bool noNormals =
        std::all_of(mesh.Vertices.begin(), mesh.Vertices.end(), [](const Vertex3D& v) { return v.Normal == Vec3{}; });
    if (noNormals && !mesh.Indices.empty()) {
        std::vector<Vertex3D> flat;
        flat.reserve(mesh.Indices.size());
        for (std::size_t t = 0; t < mesh.Indices.size(); t += 3) {
            Vertex3D a = mesh.Vertices[mesh.Indices[t]], b = mesh.Vertices[mesh.Indices[t + 1]],
                     c = mesh.Vertices[mesh.Indices[t + 2]];
            const Vec3 n = Normalize(Cross(b.Position - a.Position, c.Position - a.Position));
            a.Normal = b.Normal = c.Normal = n;
            flat.insert(flat.end(), {a, b, c});
        }
        mesh.Vertices = std::move(flat);
        mesh.Indices.clear();
        for (std::uint32_t i = 0; i < mesh.Vertices.size(); ++i) mesh.Indices.push_back(i);
    }
    mesh.Min = mesh.Max = mesh.Vertices.empty() ? Vec3{} : mesh.Vertices[0].Position;
    for (const Vertex3D& v : mesh.Vertices) {
        mesh.Min = {std::min(mesh.Min.X, v.Position.X), std::min(mesh.Min.Y, v.Position.Y),
                    std::min(mesh.Min.Z, v.Position.Z)};
        mesh.Max = {std::max(mesh.Max.X, v.Position.X), std::max(mesh.Max.Y, v.Position.Y),
                    std::max(mesh.Max.Z, v.Position.Z)};
    }
}

// ---- Poses

Vec3 Lerp3(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

// The value of a track at `t` (seconds inside the animation).
void Sample(const ModelTrack& track, float t, ModelNode& node) {
    const std::size_t count = track.Times.size();
    const std::size_t width = track.Property == TrackProperty::Rotation ? 4 : 3;
    if (count == 0 || track.Values.size() < count * width) return;
    auto value = [&](std::size_t key) { return &track.Values[key * width]; };
    std::size_t a = 0, b = 0;
    float f = 0;
    if (t <= track.Times.front()) {
        a = b = 0;
    } else if (t >= track.Times.back()) {
        a = b = count - 1;
    } else {
        b = std::size_t(std::upper_bound(track.Times.begin(), track.Times.end(), t) - track.Times.begin());
        a = b - 1;
        const float span = track.Times[b] - track.Times[a];
        f = track.Step || span <= 0 ? 0.0f : (t - track.Times[a]) / span;
    }
    const float* va = value(a);
    const float* vb = value(b);
    switch (track.Property) {
    case TrackProperty::Position:
        node.Position = Lerp3({va[0], va[1], va[2]}, {vb[0], vb[1], vb[2]}, f);
        break;
    case TrackProperty::Scale:
        node.Scale = Lerp3({va[0], va[1], va[2]}, {vb[0], vb[1], vb[2]}, f);
        break;
    case TrackProperty::Rotation:
        node.Rotation = Quat::Slerp(Quat{va[0], va[1], va[2], va[3]}, Quat{vb[0], vb[1], vb[2], vb[3]}, f);
        break;
    }
}

} // namespace

std::vector<Mat4> ComputePose(const ModelData& data, std::string_view animation, float seconds) {
    std::vector<ModelNode> nodes = data.Nodes;
    if (!animation.empty()) {
        auto found = data.Animations.find(animation);
        if (found == data.Animations.end()) {
            Log::Warn("Model: no animation '{}'", animation);
        } else {
            const ModelAnimationData& a = found->second;
            float t = seconds;
            if (a.Duration > 0)
                t = a.Loop ? std::fmod(std::fmod(t, a.Duration) + a.Duration, a.Duration)
                           : std::clamp(t, 0.0f, a.Duration);
            for (const ModelTrack& track : a.Tracks)
                if (track.Node >= 0 && track.Node < int(nodes.size())) Sample(track, t, nodes[std::size_t(track.Node)]);
        }
    }
    std::vector<Mat4> world(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const ModelNode& n = nodes[i];
        const Mat4 local = Mat4::Transform(n.Position, n.Rotation, n.Scale);
        world[i] = n.Parent >= 0 && std::size_t(n.Parent) < i ? world[std::size_t(n.Parent)] * local : local;
    }
    return world;
}

std::vector<ModelDraw>& ModelQueue() { return Queue; }
std::vector<PointLight3D>& PointLightQueue() { return Lights; }

void ResetModels() {
    Queue.clear();
    Lights.clear();
}

} // namespace Detail

// ---------------------------------------------------------------- Mesh

Mesh Mesh::Create(std::vector<Vertex3D> vertices, std::vector<std::uint32_t> indices) {
    Mesh mesh;
    mesh.Data = std::make_shared<Detail::MeshData>();
    mesh.Data->Vertices = std::move(vertices);
    mesh.Data->Indices = std::move(indices);
    Detail::FinishMesh(*mesh.Data);
    return mesh;
}

Mesh Mesh::Cube(Vec3 size) {
    const Vec3 h = size * 0.5f;
    std::vector<Vertex3D> v;
    std::vector<std::uint32_t> idx;
    // Each face: its normal and two axes along it, so that the corners go counter-clockwise from outside.
    const Vec3 faces[6][3] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                              {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                              {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
    for (const auto& f : faces) {
        const Vec3 n = f[0], u = f[1], w = f[2];
        const auto base = std::uint32_t(v.size());
        const Vec2 corners[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const Vec2 c : corners)
            v.push_back({(n + u * c.X + w * c.Y) * h, n, {(c.X + 1) * 0.5f, (1 - c.Y) * 0.5f}});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return Create(std::move(v), std::move(idx));
}

Mesh Mesh::Sphere(float radius, int segments) {
    segments = std::max(3, segments);
    const int rings = std::max(2, segments / 2);
    std::vector<Vertex3D> v;
    std::vector<std::uint32_t> idx;
    for (int r = 0; r <= rings; ++r) {
        const float phi = Pi * float(r) / float(rings); // 0 at the top
        for (int s = 0; s <= segments; ++s) {
            const float theta = 2 * Pi * float(s) / float(segments);
            const Vec3 n{std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta)};
            v.push_back({n * radius, n, {float(s) / float(segments), float(r) / float(rings)}});
        }
    }
    const auto row = std::uint32_t(segments + 1);
    for (std::uint32_t r = 0; r < std::uint32_t(rings); ++r)
        for (std::uint32_t s = 0; s < std::uint32_t(segments); ++s) {
            const std::uint32_t a = r * row + s, b = a + row;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    return Create(std::move(v), std::move(idx));
}

Mesh Mesh::Plane(Vec2 size) {
    const float x = size.X * 0.5f, z = size.Y * 0.5f;
    const Vec3 up{0, 1, 0};
    std::vector<Vertex3D> v = {
        {{-x, 0, z}, up, {0, 1}}, {{x, 0, z}, up, {1, 1}}, {{x, 0, -z}, up, {1, 0}}, {{-x, 0, -z}, up, {0, 0}}};
    return Create(std::move(v), {0, 1, 2, 0, 2, 3});
}

Mesh Mesh::Cylinder(float radius, float height, int segments) {
    segments = std::max(3, segments);
    const float h = height * 0.5f;
    std::vector<Vertex3D> v;
    std::vector<std::uint32_t> idx;
    for (int s = 0; s <= segments; ++s) { // the side
        const float theta = 2 * Pi * float(s) / float(segments);
        const Vec3 n{std::sin(theta), 0, std::cos(theta)};
        v.push_back({{n.X * radius, -h, n.Z * radius}, n, {float(s) / float(segments), 1}});
        v.push_back({{n.X * radius, h, n.Z * radius}, n, {float(s) / float(segments), 0}});
    }
    for (std::uint32_t s = 0; s < std::uint32_t(segments); ++s) {
        const std::uint32_t a = s * 2;
        idx.insert(idx.end(), {a, a + 2, a + 1, a + 1, a + 2, a + 3});
    }
    for (int cap = 0; cap < 2; ++cap) { // bottom and top
        const float y = cap ? h : -h;
        const Vec3 n{0, cap ? 1.0f : -1.0f, 0};
        const auto centre = std::uint32_t(v.size());
        v.push_back({{0, y, 0}, n, {0.5f, 0.5f}});
        for (int s = 0; s <= segments; ++s) {
            const float theta = 2 * Pi * float(s) / float(segments);
            v.push_back({{std::sin(theta) * radius, y, std::cos(theta) * radius},
                         n,
                         {0.5f + std::sin(theta) * 0.5f, 0.5f + std::cos(theta) * 0.5f}});
        }
        for (std::uint32_t s = 0; s < std::uint32_t(segments); ++s) {
            if (cap)
                idx.insert(idx.end(), {centre, centre + 1 + s, centre + 2 + s});
            else
                idx.insert(idx.end(), {centre, centre + 2 + s, centre + 1 + s});
        }
    }
    return Create(std::move(v), std::move(idx));
}

const std::vector<Vertex3D>& Mesh::Vertices() const {
    static const std::vector<Vertex3D> none;
    return Data ? Data->Vertices : none;
}
const std::vector<std::uint32_t>& Mesh::Indices() const {
    static const std::vector<std::uint32_t> none;
    return Data ? Data->Indices : none;
}
Vec3 Mesh::Min() const { return Data ? Data->Min : Vec3{}; }
Vec3 Mesh::Max() const { return Data ? Data->Max : Vec3{}; }

// ---------------------------------------------------------------- Model

bool ModelAnimation::Fired(std::string_view event, float from, float to) const {
    if (to <= from) return false;
    for (const auto& [name, time] : Events) {
        if (name != event) continue;
        if (Looping && Seconds > 0) {
            // The first pass of the event at or after `from`.
            const float k = std::ceil((from - time) / Seconds);
            if (time + k * Seconds < to) return true;
        } else if (time >= from && time < to) {
            return true;
        }
    }
    return false;
}

Model Model::Load(const std::filesystem::path& path) {
    const std::filesystem::path full = path.is_relative() ? Files::AssetPath(path) : path;
    auto text = Files::Read(full);
    if (!text) {
        Log::Error("Cannot read the model {}", Detail::Utf8(full));
        return {};
    }
    return Parse(*text, full.parent_path(), Detail::Utf8(path));
}

Model Model::Parse(std::string_view json, const std::filesystem::path& folder, std::string_view name) {
    const std::optional<ModelFile> file = ModelFile::Parse(json, name);
    if (!file) return {};
    return FromFile(*file, folder, name);
}

Model Model::FromMesh(const Mesh& mesh, const Material& material) {
    Model model;
    model.Data = std::make_shared<Detail::ModelData>();
    model.Data->Meshes.push_back({mesh, material});
    Detail::ModelNode node;
    node.Mesh = 0;
    model.Data->Nodes.push_back(node);
    return model;
}

ModelAnimation Model::Animation(std::string_view name) const {
    ModelAnimation animation;
    if (!Data) return animation;
    auto found = Data->Animations.find(name);
    if (found == Data->Animations.end()) return animation;
    animation.Seconds = found->second.Duration;
    animation.Looping = found->second.Loop;
    animation.Events = found->second.Events;
    animation.Valid = true;
    return animation;
}

std::vector<std::string> Model::Animations() const {
    std::vector<std::string> names;
    if (Data)
        for (const auto& [name, a] : Data->Animations) names.push_back(name);
    return names;
}

std::vector<std::string> Model::Nodes() const {
    std::vector<std::string> names;
    if (Data)
        for (const Detail::ModelNode& n : Data->Nodes) names.push_back(n.Name);
    return names;
}

std::vector<Mat4> Model::Pose(std::string_view animation, float seconds) const {
    return Data ? Detail::ComputePose(*Data, animation, seconds) : std::vector<Mat4>{};
}

std::optional<Mat4> Model::Socket(std::string_view name, std::string_view animation, float seconds) const {
    if (!Data) return std::nullopt;
    auto found = Data->Sockets.find(name);
    if (found == Data->Sockets.end()) return std::nullopt;
    const std::vector<Mat4> pose = Pose(animation, seconds);
    const Detail::ModelSocket& s = found->second;
    return pose[std::size_t(s.Node)] * Mat4::Transform(s.Position, s.Rotation);
}

// ---------------------------------------------------------------- Camera3D

Mat4 Camera3D::View() const { return Mat4::LookAt(Position, Target, Up); }

Mat4 Camera3D::Projection(float aspect) const {
    if (Orthographic) {
        const float h = Height * 0.5f, w = h * aspect;
        return Mat4::Ortho(-w, w, -h, h, Near, Far);
    }
    return Mat4::Perspective(Fov * Pi / 180.0f, aspect, Near, Far);
}

Mat4 Camera3D::ViewProjection() const {
    const Vec2 size = Screen::Size();
    const float aspect = size.Y > 0 ? size.X / size.Y : 1.0f;
    return Projection(aspect) * View();
}

std::optional<Vec2> Camera3D::ToScreen(Vec3 world) const {
    const Vec4 clip = ViewProjection() * Vec4(world, 1);
    if (clip.W <= 1e-6f) return std::nullopt;
    const Vec2 ndc{clip.X / clip.W, clip.Y / clip.W};
    const Vec2 size = Screen::Size();
    return Vec2{(ndc.X + 1) * 0.5f * size.X, (1 - ndc.Y) * 0.5f * size.Y};
}

Vec3 Camera3D::Ray(Vec2 screen) const {
    const Vec2 size = Screen::Size();
    if (size.X <= 0 || size.Y <= 0) return Normalize(Target - Position);
    const Vec2 ndc{screen.X / size.X * 2 - 1, 1 - screen.Y / size.Y * 2};
    const Mat4 inverse = ViewProjection().Inverse();
    const Vec3 nearPoint = inverse.TransformPoint(Vec3(ndc, 0)), farPoint = inverse.TransformPoint(Vec3(ndc, 1));
    return Normalize(farPoint - nearPoint);
}

// ---------------------------------------------------------------- Screen and Lighting3D

namespace Screen {

void SetCamera3D(const Camera3D& camera) { Detail::Camera = camera; }
const Camera3D& CurrentCamera3D() { return Detail::Camera; }

void Draw(const Model3D& model) {
    if (!model.Source) return;
    const Detail::ModelData& data = *model.Source.Internal();
    const std::vector<Mat4> pose = Detail::ComputePose(data, model.Animation, model.Time);
    const Mat4 placed = Mat4::Transform(model.Position, model.Rotation, model.Scale);
    for (std::size_t i = 0; i < data.Nodes.size(); ++i) {
        const int mesh = data.Nodes[i].Mesh;
        if (mesh < 0 || mesh >= int(data.Meshes.size())) continue;
        const Detail::ModelPart& part = data.Meshes[std::size_t(mesh)];
        if (!part.Geometry) continue;
        Detail::Queue.push_back({part.Geometry.Internal(), part.Look, placed * pose[i], model.Tint});
    }
}

void Draw(const PointLight3D& light) { Detail::Lights.push_back(light); }

} // namespace Screen

namespace Lighting3D {

void SetSun(Vec3 direction, Color color, float intensity) {
    const Vec3 d = Normalize(direction);
    Detail::SunDirectionValue = Length(d) > 0 ? d : Vec3(0, -1, 0);
    Detail::SunColorValue = color;
    Detail::SunIntensity = intensity;
}
Vec3 SunDirection() { return Detail::SunDirectionValue; }
Color SunColor() {
    const Color c = Detail::SunColorValue;
    return {c.R * Detail::SunIntensity, c.G * Detail::SunIntensity, c.B * Detail::SunIntensity, 1};
}
void SetAmbient(Color ambient) { Detail::AmbientValue = ambient; }
Color Ambient() { return Detail::AmbientValue; }

} // namespace Lighting3D

} // namespace BitMotor
