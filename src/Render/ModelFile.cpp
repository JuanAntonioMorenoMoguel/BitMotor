// .bitmodel files as data: reading them, writing them back, and turning them into a Model to draw.
#include "BitMotor/Render/ModelFile.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "Core/FilesInternal.h"
#include "Render/ModelInternal.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace BitMotor {

namespace {

using Json = nlohmann::json;

struct Reader {
    std::string Name;

    void Unknown(const Json& object, std::initializer_list<const char*> known, std::string_view where) const {
        if (!object.is_object()) return;
        for (auto& [key, value] : object.items()) {
            (void)value;
            if (std::none_of(known.begin(), known.end(), [&](const char* k) { return key == k; }))
                Log::Warn("{}: unknown key '{}' in {}", Name, key, where);
        }
    }

    static float Number(const Json& v, float fallback = 0) { return v.is_number() ? v.get<float>() : fallback; }

    std::string Text(const Json& object, const char* key, std::string_view where) const {
        if (!object.contains(key)) return {};
        const Json& v = object[key];
        if (!v.is_string()) {
            Log::Warn("{}: {}.{} must be text", Name, where, key);
            return {};
        }
        return v.get<std::string>();
    }

    bool Bool(const Json& object, const char* key, bool fallback, std::string_view where) const {
        if (!object.contains(key)) return fallback;
        const Json& v = object[key];
        if (!v.is_boolean()) {
            Log::Warn("{}: {}.{} must be true or false", Name, where, key);
            return fallback;
        }
        return v.get<bool>();
    }

    std::vector<float> Numbers(const Json& object, const char* key, std::string_view where) const {
        std::vector<float> out;
        if (!object.contains(key)) return out;
        const Json& list = object[key];
        if (!list.is_array()) {
            Log::Warn("{}: {}.{} must be a list of numbers", Name, where, key);
            return out;
        }
        out.reserve(list.size());
        for (const Json& v : list) out.push_back(Number(v));
        return out;
    }

    Vec3 Vector(const Json& object, const char* key, Vec3 fallback) const {
        if (!object.contains(key)) return fallback;
        const Json& v = object[key];
        if (!v.is_array() || v.size() != 3) {
            Log::Warn("{}: {} must be [X, Y, Z]", Name, key);
            return fallback;
        }
        return {Number(v[0]), Number(v[1]), Number(v[2])};
    }

    Quat Rotation(const Json& object, const char* key) const {
        if (!object.contains(key)) return {};
        const Json& v = object[key];
        if (!v.is_array() || v.size() != 4) {
            Log::Warn("{}: {} must be a quaternion [X, Y, Z, W]", Name, key);
            return {};
        }
        return Quat{Number(v[0]), Number(v[1]), Number(v[2]), Number(v[3], 1)}.Normalized();
    }

    Color ColorOf(const Json& object, const char* key) const {
        if (!object.contains(key)) return Colors::White;
        const Json& v = object[key];
        if (!v.is_array() || (v.size() != 3 && v.size() != 4)) {
            Log::Warn("{}: {} must be [R, G, B, A]", Name, key);
            return Colors::White;
        }
        return {Number(v[0]), Number(v[1]), Number(v[2]), v.size() == 4 ? Number(v[3], 1) : 1.0f};
    }
};

// ---- writing

// At most 6 decimals, no trailing zeros, never "-0": the same float always gives the same text.
std::string NumberText(float value) {
    if (!std::isfinite(value)) value = 0;
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.6f", double(value));
    std::string text = buffer;
    if (text.find('.') != std::string::npos) {
        while (text.back() == '0') text.pop_back();
        if (text.back() == '.') text.pop_back();
    }
    if (text == "-0") text = "0";
    return text;
}

std::string Quoted(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof buffer, "\\u%04x", unsigned(c));
                out += buffer;
            } else {
                out += c;
            }
        }
    }
    return out + "\"";
}

std::string List(std::initializer_list<float> values) {
    std::string out = "[";
    bool first = true;
    for (const float v : values) {
        if (!first) out += ", ";
        out += NumberText(v);
        first = false;
    }
    return out + "]";
}

std::string List(const std::vector<float>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) out += (i ? ", " : "") + NumberText(values[i]);
    return out + "]";
}

std::string Vector(Vec3 v) { return List({v.X, v.Y, v.Z}); }
std::string Rotation(Quat q) { return List({q.X, q.Y, q.Z, q.W}); }

// `"Key": value` pairs joined on one line: {"A": 1, "B": 2}.
struct Line {
    std::string Text;
    void Add(std::string_view key, const std::string& value) {
        Text += Text.empty() ? "{" : ", ";
        Text += Quoted(key) + ": " + value;
    }
    std::string Done() const { return Text.empty() ? "{}" : Text + "}"; }
};

// A long list of numbers, `width` per line.
template <class Value> std::string Block(const std::vector<Value>& values, std::size_t width, std::string_view indent) {
    std::string out = "[\n";
    for (std::size_t i = 0; i < values.size(); i += width) {
        out += indent;
        out += "  ";
        for (std::size_t k = i; k < std::min(values.size(), i + width); ++k) {
            if (k > i) out += ", ";
            if constexpr (std::is_floating_point_v<Value>)
                out += NumberText(values[k]);
            else
                out += std::to_string(values[k]);
        }
        out += i + width < values.size() ? ",\n" : "\n";
    }
    return out + std::string(indent) + "]";
}

const char* PropertyName(ModelFile::Property p) {
    switch (p) {
    case ModelFile::Property::Position:
        return "Position";
    case ModelFile::Property::Rotation:
        return "Rotation";
    case ModelFile::Property::Scale:
        return "Scale";
    }
    return "Position";
}

} // namespace

// ---------------------------------------------------------------- the data

bool ModelFile::Mesh::operator==(const Mesh& other) const {
    if (Material != other.Material || Indices != other.Indices || Vertices.size() != other.Vertices.size())
        return false;
    for (std::size_t i = 0; i < Vertices.size(); ++i) {
        const Vertex3D &a = Vertices[i], &b = other.Vertices[i];
        if (a.Position != b.Position || a.Normal != b.Normal || a.Uv != b.Uv || a.Tint != b.Tint) return false;
    }
    return true;
}

float ModelFile::Animation::Length() const {
    if (Duration) return *Duration;
    float last = 0;
    for (const Track& t : Tracks)
        if (!t.Times.empty()) last = std::max(last, t.Times.back());
    return last;
}

int ModelFile::FindNode(std::string_view name) const {
    for (std::size_t i = 0; i < Nodes.size(); ++i)
        if (Nodes[i].Name == name) return int(i);
    return -1;
}

// ---------------------------------------------------------------- reading

std::optional<ModelFile> ModelFile::Load(const std::filesystem::path& path) {
    const std::filesystem::path full = path.is_relative() ? Files::AssetPath(path) : path;
    auto text = Files::Read(full);
    if (!text) {
        Log::Error("Cannot read the model {}", Detail::Utf8(full));
        return std::nullopt;
    }
    return Parse(*text, Detail::Utf8(path));
}

std::optional<ModelFile> ModelFile::Parse(std::string_view text, std::string_view name) {
    Json json = Json::parse(text, nullptr, false);
    const Reader r{std::string(name)};
    if (json.is_discarded() || !json.is_object()) {
        Log::Error("{}: not valid JSON", name);
        return std::nullopt;
    }
    if (json.value("Format", std::string()) != "bitmodel") {
        Log::Error("{}: \"Format\" must be \"bitmodel\"", name);
        return std::nullopt;
    }
    r.Unknown(json, {"Format", "Version", "Materials", "Meshes", "Nodes", "Animations", "Sockets"}, "the model");
    ModelFile file;
    if (json.contains("Version") && json["Version"].is_number()) {
        file.Version = int(r.Number(json["Version"], 1));
        if (file.Version > 2) Log::Warn("{}: version {} is newer than this BitMotor reads (2)", name, file.Version);
    }

    if (json.contains("Materials") && json["Materials"].is_object())
        for (auto& [key, m] : json["Materials"].items()) {
            const std::string where = "material " + key;
            r.Unknown(m,
                      {"Color", "Texture", "Filter", "Emission", "Unlit", "Metallic", "Roughness", "Normal",
                       "NormalScale", "MetalRough", "Occlusion", "OcclusionStrength", "EmissionTexture", "AlphaCutoff",
                       "DoubleSided"},
                      where);
            Material& material = file.Materials[key];
            material.Tint = r.ColorOf(m, "Color");
            material.Texture = r.Text(m, "Texture", where);
            const std::string filter = r.Text(m, "Filter", where);
            if (!filter.empty() && filter != "nearest" && filter != "linear")
                Log::Warn("{}: {}: Filter must be nearest or linear, not '{}'", name, where, filter);
            material.TextureFilter = filter == "linear" ? Filter::Linear : Filter::Nearest;
            material.Emission = m.contains("Emission") ? Reader::Number(m["Emission"]) : 0.0f;
            material.Unlit = r.Bool(m, "Unlit", false, where);
            auto number = [&](const char* key, float fallback) {
                return m.contains(key) ? Reader::Number(m[key], fallback) : fallback;
            };
            material.Metallic = std::clamp(number("Metallic", 0), 0.0f, 1.0f);
            material.Roughness = std::clamp(number("Roughness", 1), 0.0f, 1.0f);
            material.Normal = r.Text(m, "Normal", where);
            material.NormalScale = number("NormalScale", 1);
            material.MetalRough = r.Text(m, "MetalRough", where);
            material.Occlusion = r.Text(m, "Occlusion", where);
            material.OcclusionStrength = std::clamp(number("OcclusionStrength", 1), 0.0f, 1.0f);
            material.EmissionTexture = r.Text(m, "EmissionTexture", where);
            material.AlphaCutoff = std::clamp(number("AlphaCutoff", 0), 0.0f, 1.0f);
            material.DoubleSided = r.Bool(m, "DoubleSided", false, where);
        }

    if (json.contains("Meshes") && json["Meshes"].is_object())
        for (auto& [key, m] : json["Meshes"].items()) {
            const std::string where = "mesh " + key;
            r.Unknown(m, {"Material", "Positions", "Normals", "Uvs", "Colors", "Indices"}, where);
            const std::vector<float> positions = r.Numbers(m, "Positions", where),
                                     normals = r.Numbers(m, "Normals", where), uvs = r.Numbers(m, "Uvs", where),
                                     colors = r.Numbers(m, "Colors", where);
            const std::size_t count = positions.size() / 3;
            if (positions.size() % 3 != 0 || count == 0) {
                Log::Warn("{}: {} needs Positions, three numbers per vertex", name, where);
                continue;
            }
            const bool hasNormals = normals.size() == count * 3, hasUvs = uvs.size() == count * 2,
                       hasColors = colors.size() == count * 4;
            if (!normals.empty() && !hasNormals) Log::Warn("{}: {}: Normals do not match the vertices", name, where);
            if (!uvs.empty() && !hasUvs) Log::Warn("{}: {}: Uvs do not match the vertices", name, where);
            if (!colors.empty() && !hasColors) Log::Warn("{}: {}: Colors do not match the vertices", name, where);
            Mesh& mesh = file.Meshes[key];
            mesh.Material = r.Text(m, "Material", where);
            mesh.Vertices.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                Vertex3D& v = mesh.Vertices[i];
                v.Position = {positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]};
                if (hasNormals) v.Normal = {normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
                if (hasUvs) v.Uv = {uvs[i * 2], uvs[i * 2 + 1]};
                if (hasColors) v.Tint = {colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3]};
            }
            if (m.contains("Indices") && m["Indices"].is_array()) {
                for (const Json& i : m["Indices"]) {
                    if (i.is_number_integer() && i.get<long long>() >= 0)
                        mesh.Indices.push_back(i.get<std::uint32_t>());
                    else
                        Log::Warn("{}: {}: Indices must be whole numbers from 0", name, where);
                }
            }
        }

    if (json.contains("Nodes") && json["Nodes"].is_array())
        for (const Json& n : json["Nodes"]) {
            r.Unknown(n, {"Name", "Parent", "Mesh", "Position", "Rotation", "Scale"}, "a node");
            Node node;
            node.Name = r.Text(n, "Name", "a node");
            node.Parent = n.contains("Parent") && n["Parent"].is_number_integer() ? n["Parent"].get<int>() : -1;
            if (node.Parent >= int(file.Nodes.size()) || node.Parent < -1) {
                Log::Warn("{}: node '{}' has a parent that comes after it (ignored)", name, node.Name);
                node.Parent = -1;
            }
            node.Mesh = r.Text(n, "Mesh", "node " + node.Name);
            node.Position = r.Vector(n, "Position", {0, 0, 0});
            node.Rotation = r.Rotation(n, "Rotation");
            node.Scale = r.Vector(n, "Scale", {1, 1, 1});
            file.Nodes.push_back(std::move(node));
        }

    if (json.contains("Animations") && json["Animations"].is_object())
        for (auto& [key, a] : json["Animations"].items()) {
            const std::string where = "animation " + key;
            r.Unknown(a, {"Duration", "Loop", "Tracks", "Events"}, where);
            Animation& animation = file.Animations[key];
            animation.Loop = r.Bool(a, "Loop", true, where);
            if (a.contains("Duration") && a["Duration"].is_number()) animation.Duration = Reader::Number(a["Duration"]);
            if (a.contains("Tracks") && a["Tracks"].is_array())
                for (const Json& t : a["Tracks"]) {
                    r.Unknown(t, {"Node", "Property", "Interpolation", "Times", "Values"}, where);
                    Track track;
                    track.Node = r.Text(t, "Node", where);
                    const std::string property = r.Text(t, "Property", where);
                    if (property == "Position")
                        track.Property = Property::Position;
                    else if (property == "Rotation")
                        track.Property = Property::Rotation;
                    else if (property == "Scale")
                        track.Property = Property::Scale;
                    else {
                        Log::Warn("{}: {}: Property must be Position, Rotation or Scale, not '{}'", name, where,
                                  property);
                        continue;
                    }
                    const std::string interpolation = r.Text(t, "Interpolation", where);
                    if (!interpolation.empty() && interpolation != "Linear" && interpolation != "Step")
                        Log::Warn("{}: {}: Interpolation must be Linear or Step, not '{}'", name, where, interpolation);
                    track.Step = interpolation == "Step";
                    track.Times = r.Numbers(t, "Times", where);
                    track.Values = r.Numbers(t, "Values", where);
                    if (!std::is_sorted(track.Times.begin(), track.Times.end()))
                        Log::Warn("{}: {}: Times must grow", name, where);
                    animation.Tracks.push_back(std::move(track));
                }
            if (a.contains("Events") && a["Events"].is_object())
                for (auto& [event, times] : a["Events"].items()) {
                    std::vector<float>& list = animation.Events[event];
                    if (times.is_array())
                        for (const Json& time : times)
                            if (time.is_number()) list.push_back(time.get<float>());
                }
        }

    if (json.contains("Sockets") && json["Sockets"].is_object())
        for (auto& [key, s] : json["Sockets"].items()) {
            r.Unknown(s, {"Node", "Position", "Rotation"}, "socket " + key);
            file.Sockets[key] = {r.Text(s, "Node", "socket " + key), r.Vector(s, "Position", {0, 0, 0}),
                                 r.Rotation(s, "Rotation")};
        }
    return file;
}

bool ModelFile::Material::Physical() const {
    return Metallic != 0 || Roughness != 1 || !Normal.empty() || NormalScale != 1 || !MetalRough.empty() ||
           !Occlusion.empty() || OcclusionStrength != 1 || !EmissionTexture.empty() || AlphaCutoff != 0 ||
           DoubleSided;
}

// ---------------------------------------------------------------- writing

std::string ModelFile::ToJson() const {
    // Version 2 as soon as something needs it, so version 1 readers never get keys they do not know.
    int version = Version;
    for (const auto& [key, m] : Materials)
        if (m.Physical()) version = std::max(version, 2);
    std::string out = "{\n  \"Format\": " + Quoted(Format) + ",\n  \"Version\": " + std::to_string(version);
    auto section = [&](std::string_view key) { out += ",\n  " + Quoted(key) + ": "; };

    if (!Materials.empty()) {
        section("Materials");
        out += "{\n";
        std::size_t i = 0;
        for (const auto& [key, m] : Materials) {
            Line line;
            if (m.Tint != Colors::White) line.Add("Color", List({m.Tint.R, m.Tint.G, m.Tint.B, m.Tint.A}));
            if (!m.Texture.empty()) line.Add("Texture", Quoted(m.Texture));
            if (m.TextureFilter == Filter::Linear) line.Add("Filter", "\"linear\"");
            if (m.Emission != 0) line.Add("Emission", NumberText(m.Emission));
            if (m.Unlit) line.Add("Unlit", "true");
            if (m.Metallic != 0) line.Add("Metallic", NumberText(m.Metallic));
            if (m.Roughness != 1) line.Add("Roughness", NumberText(m.Roughness));
            if (!m.Normal.empty()) line.Add("Normal", Quoted(m.Normal));
            if (m.NormalScale != 1) line.Add("NormalScale", NumberText(m.NormalScale));
            if (!m.MetalRough.empty()) line.Add("MetalRough", Quoted(m.MetalRough));
            if (!m.Occlusion.empty()) line.Add("Occlusion", Quoted(m.Occlusion));
            if (m.OcclusionStrength != 1) line.Add("OcclusionStrength", NumberText(m.OcclusionStrength));
            if (!m.EmissionTexture.empty()) line.Add("EmissionTexture", Quoted(m.EmissionTexture));
            if (m.AlphaCutoff != 0) line.Add("AlphaCutoff", NumberText(m.AlphaCutoff));
            if (m.DoubleSided) line.Add("DoubleSided", "true");
            out += "    " + Quoted(key) + ": " + line.Done() + (++i < Materials.size() ? ",\n" : "\n");
        }
        out += "  }";
    }

    if (!Meshes.empty()) {
        section("Meshes");
        out += "{\n";
        std::size_t i = 0;
        for (const auto& [key, m] : Meshes) {
            std::vector<std::string> parts;
            if (!m.Material.empty()) parts.push_back("\"Material\": " + Quoted(m.Material));
            std::vector<float> positions, normals, uvs, colors;
            bool anyNormal = false, anyUv = false, anyColor = false;
            for (const Vertex3D& v : m.Vertices) {
                positions.insert(positions.end(), {v.Position.X, v.Position.Y, v.Position.Z});
                normals.insert(normals.end(), {v.Normal.X, v.Normal.Y, v.Normal.Z});
                uvs.insert(uvs.end(), {v.Uv.X, v.Uv.Y});
                colors.insert(colors.end(), {v.Tint.R, v.Tint.G, v.Tint.B, v.Tint.A});
                anyNormal = anyNormal || v.Normal != Vec3{};
                anyUv = anyUv || v.Uv != Vec2{};
                anyColor = anyColor || v.Tint != Colors::White;
            }
            const std::string indent = "      ";
            parts.push_back("\"Positions\": " + Block(positions, 3, indent));
            if (anyNormal) parts.push_back("\"Normals\": " + Block(normals, 3, indent));
            if (anyUv) parts.push_back("\"Uvs\": " + Block(uvs, 2, indent));
            if (anyColor) parts.push_back("\"Colors\": " + Block(colors, 4, indent));
            if (!m.Indices.empty()) parts.push_back("\"Indices\": " + Block(m.Indices, 3, indent));
            out += "    " + Quoted(key) + ": {\n";
            for (std::size_t k = 0; k < parts.size(); ++k)
                out += indent + parts[k] + (k + 1 < parts.size() ? ",\n" : "\n");
            out += std::string("    }") + (++i < Meshes.size() ? ",\n" : "\n");
        }
        out += "  }";
    }

    if (!Nodes.empty()) {
        section("Nodes");
        out += "[\n";
        for (std::size_t i = 0; i < Nodes.size(); ++i) {
            const Node& n = Nodes[i];
            Line line;
            line.Add("Name", Quoted(n.Name));
            line.Add("Parent", std::to_string(n.Parent));
            if (!n.Mesh.empty()) line.Add("Mesh", Quoted(n.Mesh));
            if (n.Position != Vec3{}) line.Add("Position", Vector(n.Position));
            if (n.Rotation != Quat{}) line.Add("Rotation", Rotation(n.Rotation));
            if (n.Scale != Vec3{1, 1, 1}) line.Add("Scale", Vector(n.Scale));
            out += "    " + line.Done() + (i + 1 < Nodes.size() ? ",\n" : "\n");
        }
        out += "  ]";
    }

    if (!Animations.empty()) {
        section("Animations");
        out += "{\n";
        std::size_t i = 0;
        for (const auto& [key, a] : Animations) {
            std::vector<std::string> parts;
            if (a.Duration) parts.push_back("\"Duration\": " + NumberText(*a.Duration));
            if (!a.Loop) parts.push_back("\"Loop\": false");
            if (!a.Tracks.empty()) {
                std::string tracks = "\"Tracks\": [\n";
                for (std::size_t t = 0; t < a.Tracks.size(); ++t) {
                    const Track& track = a.Tracks[t];
                    Line line;
                    line.Add("Node", Quoted(track.Node));
                    line.Add("Property", Quoted(PropertyName(track.Property)));
                    if (track.Step) line.Add("Interpolation", "\"Step\"");
                    line.Add("Times", List(track.Times));
                    line.Add("Values", List(track.Values));
                    tracks += "        " + line.Done() + (t + 1 < a.Tracks.size() ? ",\n" : "\n");
                }
                parts.push_back(tracks + "      ]");
            }
            if (!a.Events.empty()) {
                Line line;
                for (const auto& [event, times] : a.Events) line.Add(event, List(times));
                parts.push_back("\"Events\": " + line.Done());
            }
            out += "    " + Quoted(key) + ": {\n";
            for (std::size_t k = 0; k < parts.size(); ++k)
                out += "      " + parts[k] + (k + 1 < parts.size() ? ",\n" : "\n");
            out += std::string("    }") + (++i < Animations.size() ? ",\n" : "\n");
        }
        out += "  }";
    }

    if (!Sockets.empty()) {
        section("Sockets");
        out += "{\n";
        std::size_t i = 0;
        for (const auto& [key, s] : Sockets) {
            Line line;
            line.Add("Node", Quoted(s.Node));
            if (s.Position != Vec3{}) line.Add("Position", Vector(s.Position));
            if (s.Rotation != Quat{}) line.Add("Rotation", Rotation(s.Rotation));
            out += "    " + Quoted(key) + ": " + line.Done() + (++i < Sockets.size() ? ",\n" : "\n");
        }
        out += "  }";
    }
    return out + "\n}\n";
}

bool ModelFile::Save(const std::filesystem::path& path) const {
    const std::filesystem::path full = path.is_absolute() ? path : Files::SavePath(path);
    return Files::Write(full, ToJson());
}

// ---------------------------------------------------------------- to a Model

namespace Detail {

std::string ModelNumber(float value) { return NumberText(value); }

std::shared_ptr<ModelData> BuildModel(const ModelFile& file, const std::filesystem::path& folder, std::string_view name,
                                      bool full) {
    auto data = std::make_shared<ModelData>();
    // Only the nodes, quietly (for poses).
    auto warn = [&]<class... Args>(std::format_string<Args...> format, Args&&... args) {
        if (full) Log::Warn(format, std::forward<Args>(args)...);
    };

    std::map<std::string, Material, std::less<>> materials;
    for (const auto& [key, m] : file.Materials) {
        Material material;
        material.Tint = m.Tint;
        material.Emission = m.Emission;
        material.Unlit = m.Unlit;
        material.Metallic = m.Metallic;
        material.Roughness = m.Roughness;
        material.NormalScale = m.NormalScale;
        material.OcclusionStrength = m.OcclusionStrength;
        material.AlphaCutoff = m.AlphaCutoff;
        material.DoubleSided = m.DoubleSided;
        auto load = [&](const std::string& file) {
            if (!full || file.empty()) return Texture{};
            const std::filesystem::path path = folder / Utf8Path(file);
            return Texture::Load(path.is_relative() ? Files::AssetPath(path) : path, m.TextureFilter);
        };
        material.Image = load(m.Texture);
        material.NormalMap = load(m.Normal);
        material.MetalRoughMap = load(m.MetalRough);
        material.OcclusionMap = load(m.Occlusion);
        material.EmissionMap = load(m.EmissionTexture);
        materials[key] = material;
    }

    std::map<std::string, int, std::less<>> meshIndex;
    for (const auto& [key, m] : file.Meshes) {
        if (!full || m.Vertices.empty()) continue;
        ModelPart part;
        part.Geometry = Mesh::Create(m.Vertices, m.Indices);
        auto found = materials.find(m.Material);
        if (found != materials.end())
            part.Look = found->second;
        else if (!m.Material.empty())
            warn("{}: mesh {} uses the material '{}', which does not exist", name, key, m.Material);
        meshIndex[key] = int(data->Meshes.size());
        data->Meshes.push_back(std::move(part));
    }

    std::map<std::string, int, std::less<>> nodeIndex;
    for (const ModelFile::Node& n : file.Nodes) {
        ModelNode node;
        node.Name = n.Name;
        node.Parent = n.Parent >= 0 && n.Parent < int(data->Nodes.size()) ? n.Parent : -1;
        if (n.Parent != node.Parent) warn("{}: node '{}' has a parent that comes after it (ignored)", name, node.Name);
        if (!n.Mesh.empty()) {
            auto found = meshIndex.find(n.Mesh);
            if (found != meshIndex.end())
                node.Mesh = found->second;
            else if (full)
                warn("{}: node '{}' uses the mesh '{}', which does not exist", name, node.Name, n.Mesh);
        }
        node.Position = n.Position;
        node.Rotation = n.Rotation.Normalized();
        node.Scale = n.Scale;
        if (!node.Name.empty()) nodeIndex[node.Name] = int(data->Nodes.size());
        data->Nodes.push_back(std::move(node));
    }
    // A file with meshes and no nodes shows every mesh at the origin.
    if (data->Nodes.empty())
        for (int i = 0; i < int(data->Meshes.size()); ++i) {
            ModelNode node;
            node.Mesh = i;
            data->Nodes.push_back(node);
        }

    auto nodeNamed = [&](const std::string& n, std::string_view where) {
        auto found = nodeIndex.find(n);
        if (found == nodeIndex.end()) {
            warn("{}: {} names the node '{}', which does not exist", name, where, n);
            return -1;
        }
        return found->second;
    };

    for (const auto& [key, a] : file.Animations) {
        const std::string where = "animation " + key;
        ModelAnimationData animation;
        animation.Loop = a.Loop;
        for (const ModelFile::Track& t : a.Tracks) {
            ModelTrack track;
            track.Node = nodeNamed(t.Node, where);
            track.Property = t.Property == ModelFile::Property::Rotation ? TrackProperty::Rotation
                             : t.Property == ModelFile::Property::Scale  ? TrackProperty::Scale
                                                                         : TrackProperty::Position;
            track.Step = t.Step;
            track.Times = t.Times;
            track.Values = t.Values;
            const std::size_t width = track.Property == TrackProperty::Rotation ? 4 : 3;
            if (track.Node < 0 || track.Times.empty() || track.Values.size() != track.Times.size() * width) {
                if (track.Node >= 0) warn("{}: {}: a track needs {} Values per time", name, where, width);
                continue;
            }
            if (track.Property == TrackProperty::Rotation)
                for (std::size_t i = 0; i < track.Values.size(); i += 4) {
                    const Quat q = Quat{track.Values[i], track.Values[i + 1], track.Values[i + 2], track.Values[i + 3]}
                                       .Normalized();
                    track.Values[i] = q.X, track.Values[i + 1] = q.Y, track.Values[i + 2] = q.Z,
                    track.Values[i + 3] = q.W;
                }
            animation.Tracks.push_back(std::move(track));
        }
        float last = 0;
        for (const ModelTrack& t : animation.Tracks) last = std::max(last, t.Times.back());
        animation.Duration = a.Duration.value_or(last);
        for (const auto& [event, times] : a.Events)
            for (const float time : times) animation.Events.emplace_back(event, time);
        data->Animations[key] = std::move(animation);
    }

    for (const auto& [key, s] : file.Sockets) {
        const int node = nodeNamed(s.Node, "socket " + key);
        if (node < 0) continue;
        data->Sockets[key] = {node, s.Position, s.Rotation.Normalized()};
    }
    return data;
}

} // namespace Detail

Model Model::FromFile(const ModelFile& file, const std::filesystem::path& folder, std::string_view name) {
    Model model;
    model.Data = Detail::BuildModel(file, folder, name, true);
    return model;
}

} // namespace BitMotor
