// Wavefront OBJ (and MTL) to and from ModelFile.
#include "BitMotor/Render/Modeling.h"

#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Math/Functions.h"
#include "Render/ModelInternal.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <tuple>

namespace BitMotor::Modeling {

namespace {

std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

std::vector<std::string_view> Words(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        const std::size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
        if (i > start) out.push_back(line.substr(start, i - start));
    }
    return out;
}

float ToFloat(std::string_view s, float fallback = 0) {
    if (!s.empty() && s.front() == '+') s.remove_prefix(1);
    float value = fallback;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc() ? value : fallback;
}

long long ToInt(std::string_view s) {
    long long value = 0;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc() ? value : 0;
}

template <class F> void Lines(std::string_view text, F&& each) {
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = Trim(text.substr(start, end - start));
        if (const auto hash = line.find('#'); hash != std::string_view::npos) line = Trim(line.substr(0, hash));
        if (!line.empty()) each(line);
        start = end + 1;
    }
}

// The text after the first word (names with spaces).
std::string Rest(std::string_view line) {
    const auto space = line.find_first_of(" \t");
    return space == std::string_view::npos ? std::string() : std::string(Trim(line.substr(space)));
}

std::map<std::string, ModelFile::Material> ReadMtl(std::string_view mtl) {
    std::map<std::string, ModelFile::Material> out;
    ModelFile::Material* current = nullptr;
    Lines(mtl, [&](std::string_view line) {
        const auto w = Words(line);
        if (w[0] == "newmtl") {
            current = &out[Rest(line)];
        } else if (!current) {
        } else if (w[0] == "Kd" && w.size() >= 4) {
            current->Tint.R = ToFloat(w[1]), current->Tint.G = ToFloat(w[2]), current->Tint.B = ToFloat(w[3]);
        } else if (w[0] == "d" && w.size() >= 2) {
            current->Tint.A = ToFloat(w[1], 1);
        } else if (w[0] == "Tr" && w.size() >= 2) {
            current->Tint.A = 1 - ToFloat(w[1], 0);
        } else if (w[0] == "Ke" && w.size() >= 4) {
            current->Emission = std::max({ToFloat(w[1]), ToFloat(w[2]), ToFloat(w[3])});
        } else if (w[0] == "illum" && w.size() >= 2) {
            current->Unlit = ToInt(w[1]) == 0;
        } else if (w[0] == "map_Kd") {
            // Options (-s, -o...) come before the file name, which is last.
            current->Texture = std::string(w.back());
            std::replace(current->Texture.begin(), current->Texture.end(), '\\', '/');
        }
    });
    return out;
}

} // namespace

std::optional<ModelFile> ImportObj(std::string_view obj, std::string_view mtl, std::string_view name) {
    std::vector<Vec3> positions;
    std::vector<Color> colors;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    ModelFile file;
    file.Materials = ReadMtl(mtl);

    // One mesh per (object, material), in the order they appear.
    struct Piece {
        std::string Object, Material;
        ModelFile::Mesh Mesh;
        std::map<std::tuple<long long, long long, long long>, std::uint32_t> Seen;
    };
    std::vector<Piece> pieces;
    std::string object = "mesh", material;
    std::map<std::string, std::size_t> materialsOfObject;
    int warnings = 0;
    auto piece = [&]() -> Piece& {
        for (Piece& p : pieces)
            if (p.Object == object && p.Material == material) return p;
        pieces.push_back({object, material, {}, {}});
        return pieces.back();
    };
    auto resolve = [](long long index, std::size_t count) -> long long {
        if (index > 0) return index - 1;
        if (index < 0) return static_cast<long long>(count) + index;
        return -1;
    };

    Lines(obj, [&](std::string_view line) {
        const auto w = Words(line);
        const std::string_view kind = w[0];
        if (kind == "v" && w.size() >= 4) {
            positions.push_back({ToFloat(w[1]), ToFloat(w[2]), ToFloat(w[3])});
            colors.push_back(w.size() >= 7 ? Color{ToFloat(w[4], 1), ToFloat(w[5], 1), ToFloat(w[6], 1), 1}
                                           : Colors::White);
        } else if (kind == "vt" && w.size() >= 3) {
            uvs.push_back({ToFloat(w[1]), 1 - ToFloat(w[2])}); // OBJ's V goes up, BitMotor's down
        } else if (kind == "vt" && w.size() == 2) {
            uvs.push_back({ToFloat(w[1]), 1});
        } else if (kind == "vn" && w.size() >= 4) {
            const Vec3 n{ToFloat(w[1]), ToFloat(w[2]), ToFloat(w[3])};
            normals.push_back(LengthSquared(n) > 0 ? Normalize(n) : Vec3{});
        } else if (kind == "o" || kind == "g") {
            const std::string next = Rest(line);
            object = next.empty() ? "mesh" : next;
        } else if (kind == "usemtl") {
            material = Rest(line);
        } else if (kind == "f" && w.size() >= 4) {
            Piece& p = piece();
            std::vector<std::uint32_t> corners;
            for (std::size_t k = 1; k < w.size(); ++k) {
                std::string_view c = w[k];
                long long vi = 0, ti = 0, ni = 0;
                const auto s1 = c.find('/');
                vi = ToInt(c.substr(0, s1));
                if (s1 != std::string_view::npos) {
                    const auto s2 = c.find('/', s1 + 1);
                    ti = ToInt(c.substr(s1 + 1, s2 == std::string_view::npos ? std::string_view::npos : s2 - s1 - 1));
                    if (s2 != std::string_view::npos) ni = ToInt(c.substr(s2 + 1));
                }
                const long long v = resolve(vi, positions.size()), t = resolve(ti, uvs.size()),
                                n = resolve(ni, normals.size());
                if (v < 0 || v >= static_cast<long long>(positions.size())) {
                    ++warnings;
                    return;
                }
                const auto key = std::tuple{v, t < static_cast<long long>(uvs.size()) ? t : -1,
                                            n < static_cast<long long>(normals.size()) ? n : -1};
                auto [it, added] = p.Seen.try_emplace(key, std::uint32_t(p.Mesh.Vertices.size()));
                if (added) {
                    Vertex3D vertex;
                    vertex.Position = positions[std::size_t(v)];
                    vertex.Tint = colors[std::size_t(v)];
                    if (std::get<1>(key) >= 0) vertex.Uv = uvs[std::size_t(std::get<1>(key))];
                    if (std::get<2>(key) >= 0) vertex.Normal = normals[std::size_t(std::get<2>(key))];
                    p.Mesh.Vertices.push_back(vertex);
                }
                corners.push_back(it->second);
            }
            for (std::size_t k = 1; k + 1 < corners.size(); ++k)
                p.Mesh.Indices.insert(p.Mesh.Indices.end(), {corners[0], corners[k], corners[k + 1]});
        }
    });
    if (warnings) Log::Warn("{}: {} faces point at vertices that do not exist (skipped)", name, warnings);

    std::erase_if(pieces, [](const Piece& p) { return p.Mesh.Indices.empty(); });
    if (pieces.empty()) {
        Log::Error("{}: no faces", name);
        return std::nullopt;
    }
    std::map<std::string, int> perObject;
    for (const Piece& p : pieces) ++perObject[p.Object];
    for (Piece& p : pieces) {
        if (!p.Material.empty()) {
            if (!file.Materials.contains(p.Material)) file.Materials[p.Material] = {};
            p.Mesh.Material = p.Material;
        }
        // A mesh with normals on some vertices only: the rest would be dark, so make them all flat.
        const bool some = std::any_of(p.Mesh.Vertices.begin(), p.Mesh.Vertices.end(),
                                      [](const Vertex3D& v) { return v.Normal != Vec3{}; });
        const bool all = std::all_of(p.Mesh.Vertices.begin(), p.Mesh.Vertices.end(),
                                     [](const Vertex3D& v) { return v.Normal != Vec3{}; });
        if (some && !all) FlatNormals(p.Mesh);
        const std::string base =
            perObject[p.Object] > 1 && !p.Material.empty() ? p.Object + "." + p.Material : p.Object;
        const std::string meshName = UniqueName(file, Part::Mesh, base);
        file.Meshes[meshName] = std::move(p.Mesh);
        ModelFile::Node node;
        node.Name = UniqueName(file, Part::Node, base);
        node.Mesh = meshName;
        file.Nodes.push_back(std::move(node));
    }
    return file;
}

ObjFiles ExportObj(const ModelFile& file, std::string_view mtlName, std::string_view animation, float seconds) {
    using Detail::ModelNumber;
    ObjFiles out;
    out.Obj = "# BitMotor .bitmodel export\n";
    if (!file.Materials.empty()) out.Obj += "mtllib " + std::string(mtlName) + "\n";
    const std::vector<Mat4> pose = Pose(file, animation, seconds);
    std::size_t vertexBase = 1;
    for (std::size_t i = 0; i < file.Nodes.size(); ++i) {
        const ModelFile::Node& node = file.Nodes[i];
        auto found = file.Meshes.find(node.Mesh);
        if (found == file.Meshes.end() || found->second.Vertices.empty()) continue;
        ModelFile::Mesh mesh = found->second;
        Index(mesh);
        TransformAll(mesh, pose[i]);
        const bool colors = std::any_of(mesh.Vertices.begin(), mesh.Vertices.end(),
                                        [](const Vertex3D& v) { return v.Tint != Colors::White; });
        const bool uvs =
            std::any_of(mesh.Vertices.begin(), mesh.Vertices.end(), [](const Vertex3D& v) { return v.Uv != Vec2{}; });
        const bool normals = std::any_of(mesh.Vertices.begin(), mesh.Vertices.end(),
                                         [](const Vertex3D& v) { return v.Normal != Vec3{}; });
        out.Obj += "o " + (node.Name.empty() ? node.Mesh : node.Name) + "\n";
        if (!mesh.Material.empty() && file.Materials.contains(mesh.Material))
            out.Obj += "usemtl " + mesh.Material + "\n";
        for (const Vertex3D& v : mesh.Vertices) {
            out.Obj +=
                "v " + ModelNumber(v.Position.X) + " " + ModelNumber(v.Position.Y) + " " + ModelNumber(v.Position.Z);
            if (colors)
                out.Obj += " " + ModelNumber(v.Tint.R) + " " + ModelNumber(v.Tint.G) + " " + ModelNumber(v.Tint.B);
            out.Obj += "\n";
        }
        if (uvs)
            for (const Vertex3D& v : mesh.Vertices)
                out.Obj += "vt " + ModelNumber(v.Uv.X) + " " + ModelNumber(1 - v.Uv.Y) + "\n";
        if (normals)
            for (const Vertex3D& v : mesh.Vertices)
                out.Obj += "vn " + ModelNumber(v.Normal.X) + " " + ModelNumber(v.Normal.Y) + " " +
                           ModelNumber(v.Normal.Z) + "\n";
        for (std::size_t t = 0; t + 2 < mesh.Indices.size(); t += 3) {
            out.Obj += "f";
            for (int k = 0; k < 3; ++k) {
                const std::string n = std::to_string(vertexBase + mesh.Indices[t + std::size_t(k)]);
                out.Obj += " " + n;
                if (uvs && normals)
                    out.Obj += "/" + n + "/" + n;
                else if (uvs)
                    out.Obj += "/" + n;
                else if (normals)
                    out.Obj += "//" + n;
            }
            out.Obj += "\n";
        }
        vertexBase += mesh.Vertices.size();
    }
    for (const auto& [key, m] : file.Materials) {
        out.Mtl += "newmtl " + key + "\n";
        out.Mtl += "Kd " + ModelNumber(m.Tint.R) + " " + ModelNumber(m.Tint.G) + " " + ModelNumber(m.Tint.B) + "\n";
        if (m.Tint.A < 1) out.Mtl += "d " + ModelNumber(m.Tint.A) + "\n";
        if (m.Emission > 0) {
            const std::string e = ModelNumber(m.Emission);
            out.Mtl += "Ke " + e + " " + e + " " + e + "\n";
        }
        out.Mtl += m.Unlit ? "illum 0\n" : "illum 1\n";
        if (!m.Texture.empty()) out.Mtl += "map_Kd " + m.Texture + "\n";
        out.Mtl += "\n";
    }
    return out;
}

} // namespace BitMotor::Modeling
