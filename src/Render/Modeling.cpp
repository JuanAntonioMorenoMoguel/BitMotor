// Building and changing models as data: primitives, mesh edits, names, animation keys and checks.
#include "BitMotor/Render/Modeling.h"

#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Functions.h"
#include "Render/ModelInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <tuple>

namespace BitMotor::Modeling {

namespace {

using Key3 = std::tuple<long long, long long, long long>;

// A position rounded to a grid, so that corners in the same place compare equal.
Key3 PlaceKey(Vec3 p, float epsilon = 1e-5f) {
    return {std::llround(double(p.X) / epsilon), std::llround(double(p.Y) / epsilon),
            std::llround(double(p.Z) / epsilon)};
}

Vertex3D LerpVertex(const Vertex3D& a, const Vertex3D& b, float t) {
    Vertex3D v;
    v.Position = a.Position + (b.Position - a.Position) * t;
    const Vec3 n = a.Normal + (b.Normal - a.Normal) * t;
    v.Normal = LengthSquared(n) > 0 ? Normalize(n) : Vec3{};
    v.Uv = a.Uv + (b.Uv - a.Uv) * t;
    v.Tint = {a.Tint.R + (b.Tint.R - a.Tint.R) * t, a.Tint.G + (b.Tint.G - a.Tint.G) * t,
              a.Tint.B + (b.Tint.B - a.Tint.B) * t, a.Tint.A + (b.Tint.A - a.Tint.A) * t};
    return v;
}

// The faces sorted, each once, and only the ones that exist.
Faces Clean(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    const auto count = std::uint32_t(mesh.Indices.size() / 3);
    std::set<std::uint32_t> set;
    for (const std::uint32_t f : faces)
        if (f < count) set.insert(f);
    return {set.begin(), set.end()};
}

bool HasNormals(const ModelFile::Mesh& mesh) {
    return std::any_of(mesh.Vertices.begin(), mesh.Vertices.end(),
                       [](const Vertex3D& v) { return v.Normal != Vec3{}; });
}

void AddFace(ModelFile::Mesh& mesh, const Vertex3D& a, const Vertex3D& b, const Vertex3D& c, bool normals) {
    const auto base = std::uint32_t(mesh.Vertices.size());
    Vertex3D va = a, vb = b, vc = c;
    const Vec3 n = Cross(b.Position - a.Position, c.Position - a.Position);
    const Vec3 normal = normals && LengthSquared(n) > 0 ? Normalize(n) : Vec3{};
    va.Normal = vb.Normal = vc.Normal = normal;
    mesh.Vertices.insert(mesh.Vertices.end(), {va, vb, vc});
    mesh.Indices.insert(mesh.Indices.end(), {base, base + 1, base + 2});
}

// The normal of a transform: (inverse)^T applied to a direction.
Vec3 TransformNormal(const Mat4& inverse, Vec3 n) {
    const float* m = inverse.M; // column-major: M[column * 4 + row]
    const Vec3 r{m[0] * n.X + m[1] * n.Y + m[2] * n.Z, m[4] * n.X + m[5] * n.Y + m[6] * n.Z,
                 m[8] * n.X + m[9] * n.Y + m[10] * n.Z};
    return LengthSquared(r) > 0 ? Normalize(r) : Vec3{};
}

} // namespace

// ---------------------------------------------------------------- primitives

ModelFile::Mesh FromMesh(const BitMotor::Mesh& mesh) {
    ModelFile::Mesh out;
    out.Vertices = mesh.Vertices();
    out.Indices = mesh.Indices();
    return out;
}

BitMotor::Mesh ToMesh(const ModelFile::Mesh& mesh) { return BitMotor::Mesh::Create(mesh.Vertices, mesh.Indices); }

ModelFile::Mesh Cube(Vec3 size) { return FromMesh(BitMotor::Mesh::Cube(size)); }
ModelFile::Mesh Sphere(float radius, int segments) {
    ModelFile::Mesh mesh = FromMesh(BitMotor::Mesh::Sphere(radius, segments));
    // The rings at the poles make triangles with no area: drop them.
    Faces slivers;
    for (std::uint32_t f = 0; f < std::uint32_t(FaceCount(mesh)); ++f)
        if (FaceNormal(mesh, f) == Vec3{}) slivers.push_back(f);
    Delete(mesh, slivers);
    return mesh;
}
ModelFile::Mesh Cylinder(float radius, float height, int segments) {
    return FromMesh(BitMotor::Mesh::Cylinder(radius, height, segments));
}

ModelFile::Mesh Plane(Vec2 size, int cells) {
    cells = std::max(1, cells);
    ModelFile::Mesh mesh;
    const Vec3 up{0, 1, 0};
    for (int z = 0; z <= cells; ++z)
        for (int x = 0; x <= cells; ++x) {
            const float u = float(x) / float(cells), v = float(z) / float(cells);
            mesh.Vertices.push_back({{(u - 0.5f) * size.X, 0, (v - 0.5f) * size.Y}, up, {u, v}});
        }
    const auto row = std::uint32_t(cells + 1);
    for (std::uint32_t z = 0; z < std::uint32_t(cells); ++z)
        for (std::uint32_t x = 0; x < std::uint32_t(cells); ++x) {
            const std::uint32_t a = z * row + x, b = a + 1, c = a + row, d = c + 1;
            // Counter-clockwise seen from above: +X then -Z.
            mesh.Indices.insert(mesh.Indices.end(), {c, d, b, c, b, a});
        }
    return mesh;
}

ModelFile::Mesh Cone(float radius, float height, int segments) {
    segments = std::max(3, segments);
    const float h = height * 0.5f;
    ModelFile::Mesh mesh;
    const Vec3 tip{0, h, 0};
    for (int s = 0; s < segments; ++s) {
        const float a0 = 2 * Pi * float(s) / float(segments), a1 = 2 * Pi * float(s + 1) / float(segments);
        const Vec3 p0{std::sin(a0) * radius, -h, std::cos(a0) * radius},
            p1{std::sin(a1) * radius, -h, std::cos(a1) * radius};
        const float u0 = float(s) / float(segments), u1 = float(s + 1) / float(segments);
        AddFace(mesh, {p0, {}, {u0, 1}}, {p1, {}, {u1, 1}}, {tip, {}, {(u0 + u1) * 0.5f, 0}}, true);
        AddFace(mesh, {{0, -h, 0}, {}, {0.5f, 0.5f}},
                {p1, {}, {0.5f + std::sin(a1) * 0.5f, 0.5f + std::cos(a1) * 0.5f}},
                {p0, {}, {0.5f + std::sin(a0) * 0.5f, 0.5f + std::cos(a0) * 0.5f}}, true);
    }
    return mesh;
}

// ---------------------------------------------------------------- looking at a mesh

int FaceCount(const ModelFile::Mesh& mesh) {
    return int(mesh.Indices.empty() ? mesh.Vertices.size() / 3 : mesh.Indices.size() / 3);
}

Faces AllFaces(const ModelFile::Mesh& mesh) {
    Faces faces(std::size_t(FaceCount(mesh)));
    std::iota(faces.begin(), faces.end(), 0u);
    return faces;
}

namespace {
std::array<std::uint32_t, 3> Corners3(const ModelFile::Mesh& mesh, std::uint32_t face) {
    if (mesh.Indices.empty()) return {face * 3, face * 3 + 1, face * 3 + 2};
    return {mesh.Indices[face * 3], mesh.Indices[face * 3 + 1], mesh.Indices[face * 3 + 2]};
}
bool FaceExists(const ModelFile::Mesh& mesh, std::uint32_t face) {
    if (face >= std::uint32_t(FaceCount(mesh))) return false;
    for (const std::uint32_t i : Corners3(mesh, face))
        if (i >= mesh.Vertices.size()) return false;
    return true;
}
} // namespace

// Zero for a face with (almost) no area.
Vec3 FaceNormal(const ModelFile::Mesh& mesh, std::uint32_t face) {
    if (!FaceExists(mesh, face)) return {};
    const auto [a, b, c] = Corners3(mesh, face);
    const Vec3 n = Cross(mesh.Vertices[b].Position - mesh.Vertices[a].Position,
                         mesh.Vertices[c].Position - mesh.Vertices[a].Position);
    return LengthSquared(n) > 1e-14f ? Normalize(n) : Vec3{};
}

Vec3 FaceCentre(const ModelFile::Mesh& mesh, std::uint32_t face) {
    if (!FaceExists(mesh, face)) return {};
    const auto [a, b, c] = Corners3(mesh, face);
    return (mesh.Vertices[a].Position + mesh.Vertices[b].Position + mesh.Vertices[c].Position) / 3.0f;
}

Faces FacesFacing(const ModelFile::Mesh& mesh, Vec3 direction, float degrees) {
    Faces out;
    if (LengthSquared(direction) == 0) return out;
    const Vec3 d = Normalize(direction);
    const float limit = std::cos(degrees * Pi / 180.0f);
    for (std::uint32_t f = 0; f < std::uint32_t(FaceCount(mesh)); ++f) {
        const Vec3 n = FaceNormal(mesh, f);
        if (LengthSquared(n) > 0 && Dot(n, d) >= limit - 1e-6f) out.push_back(f);
    }
    return out;
}

std::vector<std::uint32_t> Corners(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    std::set<std::uint32_t> set;
    for (const std::uint32_t f : faces)
        if (FaceExists(mesh, f))
            for (const std::uint32_t i : Corners3(mesh, f)) set.insert(i);
    return {set.begin(), set.end()};
}

std::vector<std::uint32_t> SamePlace(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> vertices) {
    std::set<Key3> places;
    for (const std::uint32_t v : vertices)
        if (v < mesh.Vertices.size()) places.insert(PlaceKey(mesh.Vertices[v].Position));
    std::vector<std::uint32_t> out;
    for (std::uint32_t i = 0; i < mesh.Vertices.size(); ++i)
        if (places.contains(PlaceKey(mesh.Vertices[i].Position))) out.push_back(i);
    return out;
}

Faces Connected(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    const auto count = std::uint32_t(FaceCount(mesh));
    std::map<Key3, std::vector<std::uint32_t>> byPlace;
    for (std::uint32_t f = 0; f < count; ++f)
        if (FaceExists(mesh, f))
            for (const std::uint32_t i : Corners3(mesh, f)) byPlace[PlaceKey(mesh.Vertices[i].Position)].push_back(f);
    std::vector<bool> seen(count, false);
    std::vector<std::uint32_t> todo;
    for (const std::uint32_t f : faces)
        if (FaceExists(mesh, f) && !seen[f]) seen[f] = true, todo.push_back(f);
    while (!todo.empty()) {
        const std::uint32_t f = todo.back();
        todo.pop_back();
        for (const std::uint32_t i : Corners3(mesh, f))
            for (const std::uint32_t g : byPlace[PlaceKey(mesh.Vertices[i].Position)])
                if (!seen[g]) seen[g] = true, todo.push_back(g);
    }
    Faces out;
    for (std::uint32_t f = 0; f < count; ++f)
        if (seen[f]) out.push_back(f);
    return out;
}

Bounds BoundsOf(const ModelFile::Mesh& mesh) {
    if (mesh.Vertices.empty()) return {};
    Bounds b{mesh.Vertices[0].Position, mesh.Vertices[0].Position};
    for (const Vertex3D& v : mesh.Vertices) {
        b.Min = {std::min(b.Min.X, v.Position.X), std::min(b.Min.Y, v.Position.Y), std::min(b.Min.Z, v.Position.Z)};
        b.Max = {std::max(b.Max.X, v.Position.X), std::max(b.Max.Y, v.Position.Y), std::max(b.Max.Z, v.Position.Z)};
    }
    return b;
}

std::optional<Hit> Raycast(const ModelFile::Mesh& mesh, const Mat4& world, Vec3 origin, Vec3 direction) {
    std::optional<Hit> best;
    for (std::uint32_t f = 0; f < std::uint32_t(FaceCount(mesh)); ++f) {
        if (!FaceExists(mesh, f)) continue;
        const auto [ia, ib, ic] = Corners3(mesh, f);
        const Vec3 a = world.TransformPoint(mesh.Vertices[ia].Position),
                   b = world.TransformPoint(mesh.Vertices[ib].Position),
                   c = world.TransformPoint(mesh.Vertices[ic].Position);
        // Möller–Trumbore, both sides.
        const Vec3 e1 = b - a, e2 = c - a, p = Cross(direction, e2);
        const float det = Dot(e1, p);
        if (std::abs(det) < 1e-12f) continue;
        const float inv = 1 / det;
        const Vec3 s = origin - a;
        const float u = Dot(s, p) * inv;
        if (u < 0 || u > 1) continue;
        const Vec3 q = Cross(s, e1);
        const float v = Dot(direction, q) * inv;
        if (v < 0 || u + v > 1) continue;
        const float t = Dot(e2, q) * inv;
        if (t < 0) continue;
        if (!best || t < best->Distance) best = Hit{f, t, origin + direction * t};
    }
    return best;
}

// ---------------------------------------------------------------- changing a mesh

void Index(ModelFile::Mesh& mesh) {
    if (mesh.Indices.empty()) {
        for (std::uint32_t i = 0; i + 2 < mesh.Vertices.size(); i += 3)
            mesh.Indices.insert(mesh.Indices.end(), {i, i + 1, i + 2});
        return;
    }
    std::vector<std::uint32_t> kept;
    kept.reserve(mesh.Indices.size());
    for (std::size_t t = 0; t + 2 < mesh.Indices.size(); t += 3) {
        const std::uint32_t a = mesh.Indices[t], b = mesh.Indices[t + 1], c = mesh.Indices[t + 2];
        const auto n = mesh.Vertices.size();
        if (a < n && b < n && c < n) kept.insert(kept.end(), {a, b, c});
    }
    mesh.Indices = std::move(kept);
}

void RemoveUnused(ModelFile::Mesh& mesh) {
    Index(mesh);
    std::vector<std::uint32_t> remap(mesh.Vertices.size(), UINT32_MAX);
    std::vector<bool> used(mesh.Vertices.size(), false);
    for (const std::uint32_t i : mesh.Indices) used[i] = true;
    std::vector<Vertex3D> kept;
    for (std::uint32_t i = 0; i < mesh.Vertices.size(); ++i)
        if (used[i]) remap[i] = std::uint32_t(kept.size()), kept.push_back(mesh.Vertices[i]);
    for (std::uint32_t& i : mesh.Indices) i = remap[i];
    mesh.Vertices = std::move(kept);
}

void Weld(ModelFile::Mesh& mesh, float epsilon) {
    Index(mesh);
    epsilon = std::max(epsilon, 1e-9f);
    auto q = [&](float v) { return std::llround(double(v) / epsilon); };
    std::map<std::array<long long, 12>, std::uint32_t> seen;
    std::vector<Vertex3D> kept;
    std::vector<std::uint32_t> remap(mesh.Vertices.size());
    for (std::uint32_t i = 0; i < mesh.Vertices.size(); ++i) {
        const Vertex3D& v = mesh.Vertices[i];
        const std::array<long long, 12> key = {q(v.Position.X), q(v.Position.Y), q(v.Position.Z), q(v.Normal.X),
                                               q(v.Normal.Y),   q(v.Normal.Z),   q(v.Uv.X),       q(v.Uv.Y),
                                               q(v.Tint.R),     q(v.Tint.G),     q(v.Tint.B),     q(v.Tint.A)};
        auto [it, added] = seen.try_emplace(key, std::uint32_t(kept.size()));
        if (added) kept.push_back(v);
        remap[i] = it->second;
    }
    for (std::uint32_t& i : mesh.Indices) i = remap[i];
    mesh.Vertices = std::move(kept);
    RemoveUnused(mesh);
}

void Transform(ModelFile::Mesh& mesh, std::span<const std::uint32_t> vertices, const Mat4& transform) {
    const Mat4 inverse = transform.Inverse();
    std::set<std::uint32_t> done;
    for (const std::uint32_t i : vertices) {
        if (i >= mesh.Vertices.size() || !done.insert(i).second) continue;
        Vertex3D& v = mesh.Vertices[i];
        v.Position = transform.TransformPoint(v.Position);
        if (v.Normal != Vec3{}) v.Normal = TransformNormal(inverse, v.Normal);
    }
}

void TransformAll(ModelFile::Mesh& mesh, const Mat4& transform) {
    std::vector<std::uint32_t> all(mesh.Vertices.size());
    std::iota(all.begin(), all.end(), 0u);
    Transform(mesh, all, transform);
    // A mirroring transform turns the faces inside out: turn them back.
    const float* m = transform.M;
    const float det =
        m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) + m[8] * (m[1] * m[6] - m[5] * m[2]);
    if (det < 0) {
        Index(mesh);
        for (std::size_t t = 0; t + 2 < mesh.Indices.size(); t += 3)
            std::swap(mesh.Indices[t + 1], mesh.Indices[t + 2]);
    }
}

void FlatNormals(ModelFile::Mesh& mesh) {
    Index(mesh);
    ModelFile::Mesh flat;
    flat.Material = mesh.Material;
    for (std::size_t t = 0; t + 2 < mesh.Indices.size(); t += 3)
        AddFace(flat, mesh.Vertices[mesh.Indices[t]], mesh.Vertices[mesh.Indices[t + 1]],
                mesh.Vertices[mesh.Indices[t + 2]], true);
    // Faces with no area keep a zero normal; give them the up direction so the mesh never falls back to
    // flat normals as a whole.
    for (Vertex3D& v : flat.Vertices)
        if (v.Normal == Vec3{}) v.Normal = {0, 1, 0};
    mesh.Vertices = std::move(flat.Vertices);
    mesh.Indices = std::move(flat.Indices);
    Weld(mesh);
}

void SmoothNormals(ModelFile::Mesh& mesh) {
    Index(mesh);
    std::map<Key3, Vec3> sums;
    for (std::size_t t = 0; t + 2 < mesh.Indices.size(); t += 3) {
        const Vec3 a = mesh.Vertices[mesh.Indices[t]].Position, b = mesh.Vertices[mesh.Indices[t + 1]].Position,
                   c = mesh.Vertices[mesh.Indices[t + 2]].Position;
        const Vec3 n = Cross(b - a, c - a); // its length weighs it by area
        for (int k = 0; k < 3; ++k) sums[PlaceKey(mesh.Vertices[mesh.Indices[t + std::size_t(k)]].Position)] += n;
    }
    for (Vertex3D& v : mesh.Vertices) {
        const Vec3 n = sums[PlaceKey(v.Position)];
        v.Normal = LengthSquared(n) > 0 ? Normalize(n) : Vec3{0, 1, 0};
    }
    Weld(mesh);
}

void ClearNormals(ModelFile::Mesh& mesh) {
    for (Vertex3D& v : mesh.Vertices) v.Normal = {};
    Weld(mesh);
}

void Separate(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    Index(mesh);
    const Faces chosen = Clean(mesh, faces);
    std::map<std::uint32_t, std::uint32_t> copies;
    for (const std::uint32_t f : chosen)
        for (int k = 0; k < 3; ++k) {
            std::uint32_t& i = mesh.Indices[f * 3 + std::uint32_t(k)];
            auto [it, added] = copies.try_emplace(i, std::uint32_t(mesh.Vertices.size()));
            if (added) mesh.Vertices.push_back(mesh.Vertices[i]);
            i = it->second;
        }
    RemoveUnused(mesh);
}

void Flip(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    Separate(mesh, faces);
    const Faces chosen = Clean(mesh, faces);
    for (const std::uint32_t f : chosen) std::swap(mesh.Indices[f * 3 + 1], mesh.Indices[f * 3 + 2]);
    for (const std::uint32_t v : Corners(mesh, chosen)) mesh.Vertices[v].Normal = mesh.Vertices[v].Normal * -1.0f;
}

void Delete(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    Index(mesh);
    const Faces chosen = Clean(mesh, faces);
    const std::set<std::uint32_t> gone(chosen.begin(), chosen.end());
    std::vector<std::uint32_t> kept;
    for (std::uint32_t f = 0; f < mesh.Indices.size() / 3; ++f)
        if (!gone.contains(f)) kept.insert(kept.end(), mesh.Indices.begin() + f * 3, mesh.Indices.begin() + f * 3 + 3);
    mesh.Indices = std::move(kept);
    RemoveUnused(mesh);
}

Faces Extrude(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, float distance) {
    Index(mesh);
    const Faces chosen = Clean(mesh, faces);
    if (chosen.empty()) return {};
    const bool normals = HasNormals(mesh);

    Vec3 direction{};
    for (const std::uint32_t f : chosen) {
        const auto [a, b, c] = Corners3(mesh, f);
        direction += Cross(mesh.Vertices[b].Position - mesh.Vertices[a].Position,
                           mesh.Vertices[c].Position - mesh.Vertices[a].Position);
    }
    if (LengthSquared(direction) == 0) return chosen;
    direction = Normalize(direction);

    // The edges around the outside of the region: the ones whose reverse is not in it.
    std::set<std::pair<Key3, Key3>> edges;
    for (const std::uint32_t f : chosen) {
        const auto c = Corners3(mesh, f);
        for (int k = 0; k < 3; ++k)
            edges.insert({PlaceKey(mesh.Vertices[c[std::size_t(k)]].Position),
                          PlaceKey(mesh.Vertices[c[std::size_t((k + 1) % 3)]].Position)});
    }
    std::vector<std::pair<Vertex3D, Vertex3D>> border;
    for (const std::uint32_t f : chosen) {
        const auto c = Corners3(mesh, f);
        for (int k = 0; k < 3; ++k) {
            const Vertex3D &a = mesh.Vertices[c[std::size_t(k)]], &b = mesh.Vertices[c[std::size_t((k + 1) % 3)]];
            if (!edges.contains({PlaceKey(b.Position), PlaceKey(a.Position)})) border.emplace_back(a, b);
        }
    }

    // The region gets its own vertices and moves.
    Separate(mesh, chosen);
    const Vec3 offset = direction * distance;
    for (const std::uint32_t v : Corners(mesh, chosen)) mesh.Vertices[v].Position += offset;

    // The sides: a, b at the bottom and the same moved at the top, counter-clockwise from outside.
    const bool inward = distance < 0;
    for (const auto& [a, b] : border) {
        Vertex3D ta = a, tb = b;
        ta.Position += offset;
        tb.Position += offset;
        if (!inward) {
            AddFace(mesh, a, b, tb, normals);
            AddFace(mesh, a, tb, ta, normals);
        } else {
            AddFace(mesh, b, a, ta, normals);
            AddFace(mesh, b, ta, tb, normals);
        }
    }
    Weld(mesh);
    return chosen;
}

Faces Inset(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, float amount) {
    Index(mesh);
    const Faces chosen = Clean(mesh, faces);
    amount = std::clamp(amount, 0.0f, 1.0f);
    const bool normals = HasNormals(mesh);
    for (const std::uint32_t f : chosen) {
        const auto c = Corners3(mesh, f);
        const Vertex3D outer[3] = {mesh.Vertices[c[0]], mesh.Vertices[c[1]], mesh.Vertices[c[2]]};
        const Vec3 centre = (outer[0].Position + outer[1].Position + outer[2].Position) / 3.0f;
        Vertex3D inner[3];
        for (int k = 0; k < 3; ++k) {
            inner[k] = outer[k];
            inner[k].Position = outer[k].Position + (centre - outer[k].Position) * amount;
        }
        // The face itself becomes the inner one; the ring goes at the end.
        for (int k = 0; k < 3; ++k) {
            mesh.Indices[f * 3 + std::uint32_t(k)] = std::uint32_t(mesh.Vertices.size());
            mesh.Vertices.push_back(inner[k]);
        }
        for (int k = 0; k < 3; ++k) {
            const int n = (k + 1) % 3;
            AddFace(mesh, outer[k], outer[n], inner[n], normals);
            AddFace(mesh, outer[k], inner[n], inner[k], normals);
        }
    }
    Weld(mesh);
    return chosen;
}

Faces Subdivide(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces) {
    Index(mesh);
    const Faces chosen = Clean(mesh, faces);
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> middles;
    auto middle = [&](std::uint32_t a, std::uint32_t b) {
        const auto key = std::minmax(a, b);
        auto [it, added] = middles.try_emplace({key.first, key.second}, std::uint32_t(mesh.Vertices.size()));
        if (added) mesh.Vertices.push_back(LerpVertex(mesh.Vertices[key.first], mesh.Vertices[key.second], 0.5f));
        return it->second;
    };
    Faces out;
    for (const std::uint32_t f : chosen) {
        const auto [a, b, c] = Corners3(mesh, f);
        const std::uint32_t ab = middle(a, b), bc = middle(b, c), ca = middle(c, a);
        mesh.Indices[f * 3] = ab, mesh.Indices[f * 3 + 1] = bc, mesh.Indices[f * 3 + 2] = ca;
        out.push_back(f);
        for (const auto& [x, y, z] : {std::tuple{a, ab, ca}, std::tuple{ab, b, bc}, std::tuple{ca, bc, c}}) {
            out.push_back(std::uint32_t(mesh.Indices.size() / 3));
            mesh.Indices.insert(mesh.Indices.end(), {x, y, z});
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void Mirror(ModelFile::Mesh& mesh, int axis) {
    Index(mesh);
    axis = std::clamp(axis, 0, 2);
    Vec3 flip{1, 1, 1};
    (axis == 0 ? flip.X : axis == 1 ? flip.Y : flip.Z) = -1;
    ModelFile::Mesh copy = mesh;
    TransformAll(copy, Mat4::Scale(flip));
    Append(mesh, copy);
    Weld(mesh);
}

Faces Append(ModelFile::Mesh& mesh, const ModelFile::Mesh& other, const Mat4& transform) {
    Index(mesh);
    ModelFile::Mesh placed = other;
    Index(placed);
    TransformAll(placed, transform);
    const auto base = std::uint32_t(mesh.Vertices.size());
    const auto firstFace = std::uint32_t(mesh.Indices.size() / 3);
    mesh.Vertices.insert(mesh.Vertices.end(), placed.Vertices.begin(), placed.Vertices.end());
    for (const std::uint32_t i : placed.Indices) mesh.Indices.push_back(base + i);
    Faces out(placed.Indices.size() / 3);
    std::iota(out.begin(), out.end(), firstFace);
    return out;
}

void Paint(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, Color color) {
    Separate(mesh, faces);
    for (const std::uint32_t v : Corners(mesh, faces)) mesh.Vertices[v].Tint = color;
}

void PaletteUv(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, int cell, int columns, int rows) {
    columns = std::max(1, columns);
    rows = std::max(1, rows);
    cell = std::clamp(cell, 0, columns * rows - 1);
    const Vec2 uv{(float(cell % columns) + 0.5f) / float(columns), (float(cell / columns) + 0.5f) / float(rows)};
    Separate(mesh, faces);
    for (const std::uint32_t v : Corners(mesh, faces)) mesh.Vertices[v].Uv = uv;
}

void BoxUv(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, float scale) {
    Index(mesh);
    const Faces chosen = Clean(mesh, faces);
    // Every face on its own vertices: neighbours may look at different sides.
    for (const std::uint32_t f : chosen)
        for (int k = 0; k < 3; ++k) {
            std::uint32_t& i = mesh.Indices[f * 3 + std::uint32_t(k)];
            mesh.Vertices.push_back(mesh.Vertices[i]);
            i = std::uint32_t(mesh.Vertices.size() - 1);
        }
    for (const std::uint32_t f : chosen) {
        const Vec3 n = FaceNormal(mesh, f);
        const Vec3 a{std::abs(n.X), std::abs(n.Y), std::abs(n.Z)};
        for (int k = 0; k < 3; ++k) {
            Vertex3D& v = mesh.Vertices[mesh.Indices[f * 3 + std::uint32_t(k)]];
            const Vec3 p = v.Position * scale;
            if (a.Y >= a.X && a.Y >= a.Z)
                v.Uv = {n.Y >= 0 ? p.X : -p.X, p.Z};
            else if (a.X >= a.Z)
                v.Uv = {n.X >= 0 ? -p.Z : p.Z, -p.Y};
            else
                v.Uv = {n.Z >= 0 ? p.X : -p.X, -p.Y};
        }
    }
    RemoveUnused(mesh);
    Weld(mesh);
}

// ---------------------------------------------------------------- the model as a whole

bool Has(const ModelFile& file, Part part, std::string_view name) {
    const std::string key(name);
    switch (part) {
    case Part::Material:
        return file.Materials.contains(key);
    case Part::Mesh:
        return file.Meshes.contains(key);
    case Part::Node:
        return file.FindNode(name) >= 0;
    case Part::Animation:
        return file.Animations.contains(key);
    case Part::Socket:
        return file.Sockets.contains(key);
    }
    return false;
}

std::string UniqueName(const ModelFile& file, Part part, std::string_view base) {
    std::string root(base.empty() ? std::string_view("part") : base);
    if (!Has(file, part, root)) return root;
    for (int n = 2;; ++n) {
        std::string name = root + std::to_string(n);
        if (!Has(file, part, name)) return name;
    }
}

namespace {
template <class Map> bool RenameKey(Map& map, std::string_view from, std::string_view to) {
    auto found = map.find(std::string(from));
    if (found == map.end() || map.contains(std::string(to))) return false;
    auto value = std::move(found->second);
    map.erase(found);
    map.emplace(std::string(to), std::move(value));
    return true;
}
} // namespace

bool Rename(ModelFile& file, Part part, std::string_view from, std::string_view to) {
    if (to.empty() || from == to || !Has(file, part, from) || Has(file, part, to)) return false;
    switch (part) {
    case Part::Material:
        RenameKey(file.Materials, from, to);
        for (auto& [key, mesh] : file.Meshes)
            if (mesh.Material == from) mesh.Material = to;
        return true;
    case Part::Mesh:
        RenameKey(file.Meshes, from, to);
        for (ModelFile::Node& node : file.Nodes)
            if (node.Mesh == from) node.Mesh = to;
        return true;
    case Part::Node:
        for (ModelFile::Node& node : file.Nodes)
            if (node.Name == from) node.Name = to;
        for (auto& [key, animation] : file.Animations)
            for (ModelFile::Track& track : animation.Tracks)
                if (track.Node == from) track.Node = to;
        for (auto& [key, socket] : file.Sockets)
            if (socket.Node == from) socket.Node = to;
        return true;
    case Part::Animation:
        return RenameKey(file.Animations, from, to);
    case Part::Socket:
        return RenameKey(file.Sockets, from, to);
    }
    return false;
}

void RemoveNode(ModelFile& file, int node) {
    if (node < 0 || node >= int(file.Nodes.size())) return;
    const ModelFile::Node gone = file.Nodes[std::size_t(node)];
    for (ModelFile::Node& child : file.Nodes) {
        if (child.Parent != node) continue;
        // Its place in the removed node's parent: exact unless the removed node had an uneven scale and the
        // child is turned.
        child.Position = gone.Position + gone.Rotation * (gone.Scale * child.Position);
        child.Rotation = (gone.Rotation * child.Rotation).Normalized();
        child.Scale = gone.Scale * child.Scale;
        child.Parent = gone.Parent;
    }
    file.Nodes.erase(file.Nodes.begin() + node);
    for (ModelFile::Node& n : file.Nodes)
        if (n.Parent > node) --n.Parent;
    if (!gone.Name.empty() && file.FindNode(gone.Name) < 0) {
        for (auto& [key, animation] : file.Animations)
            std::erase_if(animation.Tracks, [&](const ModelFile::Track& t) { return t.Node == gone.Name; });
        std::erase_if(file.Sockets, [&](const auto& entry) { return entry.second.Node == gone.Name; });
    }
}

bool SetParent(ModelFile& file, int node, int parent) {
    const int count = int(file.Nodes.size());
    if (node < 0 || node >= count || parent < -1 || parent >= count) return false;
    for (int p = parent; p >= 0; p = file.Nodes[std::size_t(p)].Parent) {
        if (p == node) return false;
        if (file.Nodes[std::size_t(p)].Parent >= p) break; // already out of order: stop climbing
    }
    file.Nodes[std::size_t(node)].Parent = parent;
    // Parents first, otherwise in the order they had.
    std::vector<int> order;
    std::vector<bool> placed(std::size_t(count), false);
    while (int(order.size()) < count) {
        bool moved = false;
        for (int i = 0; i < count; ++i) {
            if (placed[std::size_t(i)]) continue;
            const int p = file.Nodes[std::size_t(i)].Parent;
            if (p < 0 || p >= count || placed[std::size_t(p)]) {
                placed[std::size_t(i)] = true;
                order.push_back(i);
                moved = true;
                break;
            }
        }
        if (!moved) return false; // a loop: cannot happen after the check above
    }
    std::vector<int> where(static_cast<std::size_t>(count));
    for (int k = 0; k < count; ++k) where[std::size_t(order[std::size_t(k)])] = k;
    std::vector<ModelFile::Node> nodes;
    for (const int i : order) {
        ModelFile::Node n = file.Nodes[std::size_t(i)];
        if (n.Parent >= 0 && n.Parent < count) n.Parent = where[std::size_t(n.Parent)];
        nodes.push_back(std::move(n));
    }
    file.Nodes = std::move(nodes);
    return true;
}

std::vector<Mat4> Pose(const ModelFile& file, std::string_view animation, float seconds) {
    const auto data = Detail::BuildModel(file, {}, "model", false);
    if (!animation.empty() && !data->Animations.contains(animation)) animation = {};
    return Detail::ComputePose(*data, animation, seconds);
}

namespace {
std::size_t Width(ModelFile::Property p) { return p == ModelFile::Property::Rotation ? 4 : 3; }

ModelFile::Track* FindTrack(ModelFile::Animation& a, std::string_view node, ModelFile::Property property) {
    for (ModelFile::Track& t : a.Tracks)
        if (t.Node == node && t.Property == property) return &t;
    return nullptr;
}
} // namespace

std::vector<float> ValueAt(const ModelFile& file, std::string_view animation, std::string_view node,
                           ModelFile::Property property, float seconds) {
    const int index = file.FindNode(node);
    if (index < 0) return {};
    const ModelFile::Node& rest = file.Nodes[std::size_t(index)];
    std::vector<float> value;
    switch (property) {
    case ModelFile::Property::Position:
        value = {rest.Position.X, rest.Position.Y, rest.Position.Z};
        break;
    case ModelFile::Property::Rotation:
        value = {rest.Rotation.X, rest.Rotation.Y, rest.Rotation.Z, rest.Rotation.W};
        break;
    case ModelFile::Property::Scale:
        value = {rest.Scale.X, rest.Scale.Y, rest.Scale.Z};
        break;
    }
    auto found = file.Animations.find(std::string(animation));
    if (found == file.Animations.end()) return value;
    const ModelFile::Animation& a = found->second;
    const ModelFile::Track* track = nullptr;
    for (const ModelFile::Track& t : a.Tracks)
        if (t.Node == node && t.Property == property) track = &t;
    const std::size_t w = Width(property);
    if (!track || track->Times.empty() || track->Values.size() != track->Times.size() * w) return value;
    float t = seconds;
    const float length = a.Length();
    if (length > 0) t = a.Loop ? std::fmod(std::fmod(t, length) + length, length) : std::clamp(t, 0.0f, length);
    const auto& times = track->Times;
    std::size_t i0 = 0, i1 = 0;
    float f = 0;
    if (t <= times.front()) {
        i0 = i1 = 0;
    } else if (t >= times.back()) {
        i0 = i1 = times.size() - 1;
    } else {
        i1 = std::size_t(std::upper_bound(times.begin(), times.end(), t) - times.begin());
        i0 = i1 - 1;
        const float span = times[i1] - times[i0];
        f = track->Step || span <= 0 ? 0.0f : (t - times[i0]) / span;
    }
    const float* a0 = &track->Values[i0 * w];
    const float* a1 = &track->Values[i1 * w];
    if (property == ModelFile::Property::Rotation) {
        const Quat q = Quat::Slerp(Quat{a0[0], a0[1], a0[2], a0[3]}.Normalized(),
                                   Quat{a1[0], a1[1], a1[2], a1[3]}.Normalized(), f);
        return {q.X, q.Y, q.Z, q.W};
    }
    return {a0[0] + (a1[0] - a0[0]) * f, a0[1] + (a1[1] - a0[1]) * f, a0[2] + (a1[2] - a0[2]) * f};
}

bool SetKey(ModelFile& file, std::string_view animation, std::string_view node, ModelFile::Property property,
            float seconds, std::span<const float> values) {
    const std::size_t w = Width(property);
    if (values.size() != w || node.empty()) return false;
    ModelFile::Animation& a = file.Animations[std::string(animation)];
    ModelFile::Track* track = FindTrack(a, node, property);
    if (!track) {
        ModelFile::Track added;
        added.Node = std::string(node);
        added.Property = property;
        a.Tracks.push_back(std::move(added));
        track = &a.Tracks.back();
    }
    if (track->Values.size() != track->Times.size() * w) track->Values.resize(track->Times.size() * w);
    auto& times = track->Times;
    const auto at = std::lower_bound(times.begin(), times.end(), seconds - 1e-5f);
    const std::size_t k = std::size_t(at - times.begin());
    if (at != times.end() && std::abs(*at - seconds) <= 1e-5f) {
        std::copy(values.begin(), values.end(), track->Values.begin() + std::ptrdiff_t(k * w));
    } else {
        times.insert(at, seconds);
        track->Values.insert(track->Values.begin() + std::ptrdiff_t(k * w), values.begin(), values.end());
    }
    return true;
}

bool RemoveKey(ModelFile& file, std::string_view animation, std::string_view node, ModelFile::Property property,
               float seconds) {
    auto found = file.Animations.find(std::string(animation));
    if (found == file.Animations.end()) return false;
    ModelFile::Track* track = FindTrack(found->second, node, property);
    if (!track) return false;
    const std::size_t w = Width(property);
    for (std::size_t k = 0; k < track->Times.size(); ++k) {
        if (std::abs(track->Times[k] - seconds) > 1e-5f) continue;
        track->Times.erase(track->Times.begin() + std::ptrdiff_t(k));
        if (track->Values.size() >= (k + 1) * w)
            track->Values.erase(track->Values.begin() + std::ptrdiff_t(k * w),
                                track->Values.begin() + std::ptrdiff_t((k + 1) * w));
        if (track->Times.empty())
            std::erase_if(found->second.Tracks, [&](const ModelFile::Track& t) { return &t == track; });
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- checks

std::vector<Problem> Check(const ModelFile& file) {
    std::vector<Problem> out;
    std::set<std::string> used;
    for (const auto& [key, mesh] : file.Meshes) {
        const std::string where = "mesh " + key;
        if (!mesh.Material.empty() && !file.Materials.contains(mesh.Material))
            out.push_back({Issue::MissingMaterial, where});
        if (mesh.Vertices.empty()) {
            out.push_back({Issue::EmptyMesh, where});
            continue;
        }
        const bool badCount = mesh.Indices.empty() ? mesh.Vertices.size() % 3 != 0 : mesh.Indices.size() % 3 != 0;
        if (badCount || std::any_of(mesh.Indices.begin(), mesh.Indices.end(),
                                    [&](std::uint32_t i) { return i >= mesh.Vertices.size(); }))
            out.push_back({Issue::BadIndex, where});
        for (std::uint32_t f = 0; f < std::uint32_t(FaceCount(mesh)); ++f)
            if (FaceExists(mesh, f) && FaceNormal(mesh, f) == Vec3{}) {
                out.push_back({Issue::DegenerateFace, where});
                break;
            }
    }
    std::set<std::string> names;
    for (std::size_t i = 0; i < file.Nodes.size(); ++i) {
        const ModelFile::Node& n = file.Nodes[i];
        const std::string where = "node " + n.Name;
        if (!n.Mesh.empty()) {
            used.insert(n.Mesh);
            if (!file.Meshes.contains(n.Mesh)) out.push_back({Issue::MissingMesh, where});
        }
        if (n.Parent >= int(i) || n.Parent < -1) out.push_back({Issue::ParentAfterChild, where});
        if (!n.Name.empty() && !names.insert(n.Name).second) out.push_back({Issue::DuplicateNode, where});
    }
    if (!file.Nodes.empty())
        for (const auto& [key, mesh] : file.Meshes)
            if (!used.contains(key)) out.push_back({Issue::UnusedMesh, "mesh " + key});
    for (const auto& [key, a] : file.Animations) {
        const std::string where = "animation " + key;
        for (const ModelFile::Track& t : a.Tracks) {
            if (file.FindNode(t.Node) < 0) out.push_back({Issue::MissingNode, where + " " + t.Node});
            if (t.Times.empty() || t.Values.size() != t.Times.size() * Width(t.Property))
                out.push_back({Issue::BadTrack, where + " " + t.Node});
            if (!std::is_sorted(t.Times.begin(), t.Times.end()))
                out.push_back({Issue::TimesNotGrowing, where + " " + t.Node});
        }
    }
    for (const auto& [key, s] : file.Sockets)
        if (file.FindNode(s.Node) < 0) out.push_back({Issue::MissingNode, "socket " + key});
    return out;
}

} // namespace BitMotor::Modeling
