// glTF 2.0 (.glb and .gltf) to and from ModelFile.
#include "BitMotor/Render/Modeling.h"

#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Math/Functions.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace BitMotor::Modeling {

namespace {

using Json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t GlbMagic = 0x46546C67, JsonChunk = 0x4E4F534A, BinChunk = 0x004E4942;

std::uint32_t ReadU32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
void WriteU32(Bytes& out, std::uint32_t v) {
    for (int k = 0; k < 4; ++k) out.push_back(std::uint8_t(v >> (8 * k)));
}

// glTF colours are linear; BitMotor's are sRGB.
float ToSrgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
}
float ToLinear(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

std::optional<Bytes> Base64(std::string_view text) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    Bytes out;
    unsigned buffer = 0;
    int bits = 0;
    for (const char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        const int v = value(c);
        if (v < 0) return std::nullopt;
        buffer = buffer << 6 | unsigned(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(std::uint8_t(buffer >> bits & 0xFF));
        }
    }
    return out;
}

int ComponentsOf(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT4") return 16;
    return 0;
}

struct Reader {
    const Json& Root;
    std::vector<Bytes> Buffers;
    std::vector<std::string>& Notes;

    // An accessor's values as floats (normalised integers to 0..1), `components` per element.
    std::vector<float> Floats(int index, int& components) const {
        components = 0;
        if (!Root.contains("accessors") || index < 0 || index >= int(Root["accessors"].size())) return {};
        const Json& a = Root["accessors"][std::size_t(index)];
        components = ComponentsOf(a.value("type", std::string()));
        const std::size_t count = a.value("count", std::size_t(0));
        std::vector<float> out(count * std::size_t(components), 0.0f);
        if (a.contains("sparse")) Notes.push_back("sparse accessors are read without their sparse values");
        if (!a.contains("bufferView") || components == 0) return out;
        const Json& view = Root["bufferViews"][a["bufferView"].get<std::size_t>()];
        const std::size_t buffer = view.value("buffer", std::size_t(0));
        if (buffer >= Buffers.size()) return {};
        const Bytes& data = Buffers[buffer];
        const int type = a.value("componentType", 5126);
        const bool normalized = a.value("normalized", false);
        const std::size_t size = type == 5126 || type == 5125 ? 4 : type == 5122 || type == 5123 ? 2 : 1;
        const std::size_t stride = view.value("byteStride", size * std::size_t(components));
        const std::size_t start = view.value("byteOffset", std::size_t(0)) + a.value("byteOffset", std::size_t(0));
        for (std::size_t i = 0; i < count; ++i)
            for (int k = 0; k < components; ++k) {
                const std::size_t at = start + i * stride + std::size_t(k) * size;
                if (at + size > data.size()) return {};
                const std::uint8_t* p = data.data() + at;
                float v = 0;
                switch (type) {
                case 5126: {
                    std::uint32_t bits = ReadU32(p);
                    std::memcpy(&v, &bits, 4);
                    break;
                }
                case 5125:
                    v = float(ReadU32(p));
                    break;
                case 5123: {
                    const auto u = std::uint16_t(p[0] | p[1] << 8);
                    v = normalized ? float(u) / 65535.0f : float(u);
                    break;
                }
                case 5122: {
                    const auto s = std::int16_t(std::uint16_t(p[0] | p[1] << 8));
                    v = normalized ? std::max(float(s) / 32767.0f, -1.0f) : float(s);
                    break;
                }
                case 5121:
                    v = normalized ? float(p[0]) / 255.0f : float(p[0]);
                    break;
                case 5120: {
                    const auto s = std::int8_t(p[0]);
                    v = normalized ? std::max(float(s) / 127.0f, -1.0f) : float(s);
                    break;
                }
                default:
                    return {};
                }
                out[i * std::size_t(components) + std::size_t(k)] = v;
            }
        return out;
    }

    std::vector<std::uint32_t> Indices(int index) const {
        int components = 0;
        const std::vector<float> values = Floats(index, components);
        std::vector<std::uint32_t> out;
        out.reserve(values.size());
        for (const float v : values) out.push_back(std::uint32_t(std::max(0.0f, v)));
        return out;
    }

    // A bufferView's bytes (for images).
    Bytes View(std::size_t index) const {
        const Json& view = Root["bufferViews"][index];
        const std::size_t buffer = view.value("buffer", std::size_t(0));
        if (buffer >= Buffers.size()) return {};
        const std::size_t start = view.value("byteOffset", std::size_t(0)),
                          length = view.value("byteLength", std::size_t(0));
        if (start + length > Buffers[buffer].size()) return {};
        return Bytes(Buffers[buffer].begin() + std::ptrdiff_t(start),
                     Buffers[buffer].begin() + std::ptrdiff_t(start + length));
    }
};

Vec3 Vec3Of(const Json& j, Vec3 fallback) {
    if (!j.is_array() || j.size() != 3) return fallback;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

// Position, rotation and scale of a matrix without shear.
void Decompose(const Json& m, Vec3& position, Quat& rotation, Vec3& scale) {
    float a[16];
    for (int i = 0; i < 16; ++i) a[i] = m[std::size_t(i)].get<float>();
    position = {a[12], a[13], a[14]};
    Vec3 x{a[0], a[1], a[2]}, y{a[4], a[5], a[6]}, z{a[8], a[9], a[10]};
    scale = {Length(x), Length(y), Length(z)};
    if (Dot(Cross(x, y), z) < 0) scale.X = -scale.X;
    if (scale.X != 0) x = x / scale.X;
    if (scale.Y != 0) y = y / scale.Y;
    if (scale.Z != 0) z = z / scale.Z;
    const float trace = x.X + y.Y + z.Z;
    Quat q;
    if (trace > 0) {
        const float s = std::sqrt(trace + 1) * 2;
        q = {(y.Z - z.Y) / s, (z.X - x.Z) / s, (x.Y - y.X) / s, s / 4};
    } else if (x.X > y.Y && x.X > z.Z) {
        const float s = std::sqrt(1 + x.X - y.Y - z.Z) * 2;
        q = {s / 4, (y.X + x.Y) / s, (z.X + x.Z) / s, (y.Z - z.Y) / s};
    } else if (y.Y > z.Z) {
        const float s = std::sqrt(1 + y.Y - x.X - z.Z) * 2;
        q = {(y.X + x.Y) / s, s / 4, (z.Y + y.Z) / s, (z.X - x.Z) / s};
    } else {
        const float s = std::sqrt(1 + z.Z - x.X - y.Y) * 2;
        q = {(z.X + x.Z) / s, (z.Y + y.Z) / s, s / 4, (x.Y - y.X) / s};
    }
    rotation = q.Normalized();
}

std::string UniqueIn(std::set<std::string>& taken, std::string base) {
    if (base.empty()) base = "part";
    std::string name = base;
    for (int n = 2; taken.contains(name); ++n) name = base + std::to_string(n);
    taken.insert(name);
    return name;
}

} // namespace

std::optional<GltfImport> ImportGltf(std::span<const std::uint8_t> bytes, const GltfFiles& files,
                                     std::string_view name) {
    std::string_view text;
    Bytes bin;
    bool glb = false;
    if (bytes.size() >= 12 && ReadU32(bytes.data()) == GlbMagic) {
        glb = true;
        std::size_t at = 12;
        while (at + 8 <= bytes.size()) {
            const std::uint32_t length = ReadU32(bytes.data() + at), type = ReadU32(bytes.data() + at + 4);
            at += 8;
            if (at + length > bytes.size()) break;
            if (type == JsonChunk) text = std::string_view(reinterpret_cast<const char*>(bytes.data() + at), length);
            if (type == BinChunk)
                bin.assign(bytes.begin() + std::ptrdiff_t(at), bytes.begin() + std::ptrdiff_t(at + length));
            at += (length + 3) / 4 * 4;
        }
    } else {
        text = std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    const Json root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("asset")) {
        Log::Error("{}: not a glTF file", name);
        return std::nullopt;
    }
    if (root["asset"].value("version", std::string()).rfind("2", 0) != 0) {
        Log::Error("{}: only glTF 2.0 can be read", name);
        return std::nullopt;
    }

    GltfImport result;
    ModelFile& file = result.File;
    std::vector<std::string>& notes = result.Notes;
    Reader r{root, {}, notes};

    if (root.contains("buffers"))
        for (std::size_t i = 0; i < root["buffers"].size(); ++i) {
            const Json& b = root["buffers"][i];
            const std::string uri = b.value("uri", std::string());
            if (uri.empty()) {
                r.Buffers.push_back(glb && i == 0 ? bin : Bytes{});
            } else if (uri.rfind("data:", 0) == 0) {
                const auto comma = uri.find(',');
                r.Buffers.push_back(comma == std::string::npos
                                        ? Bytes{}
                                        : Base64(std::string_view(uri).substr(comma + 1)).value_or(Bytes{}));
            } else {
                std::optional<Bytes> data = files ? files(uri) : std::nullopt;
                if (!data) {
                    Log::Error("{}: cannot read the buffer {}", name, uri);
                    return std::nullopt;
                }
                r.Buffers.push_back(std::move(*data));
            }
        }

    // ---- images and materials
    std::set<std::string> takenMaterials;
    std::vector<std::string> imageNames;
    if (root.contains("images"))
        for (std::size_t i = 0; i < root["images"].size(); ++i) {
            const Json& image = root["images"][i];
            const std::string uri = image.value("uri", std::string()), mime = image.value("mimeType", std::string());
            const bool jpeg = mime == "image/jpeg" || uri.ends_with(".jpg") || uri.ends_with(".jpeg") ||
                              uri.rfind("data:image/jpeg", 0) == 0;
            if (!uri.empty() && uri.rfind("data:", 0) != 0) {
                imageNames.push_back(uri);
                continue;
            }
            Bytes data;
            if (!uri.empty()) {
                const auto comma = uri.find(',');
                if (comma != std::string::npos)
                    data = Base64(std::string_view(uri).substr(comma + 1)).value_or(Bytes{});
            } else if (image.contains("bufferView")) {
                data = r.View(image["bufferView"].get<std::size_t>());
            }
            const std::string imageName = std::string(name) + "_" + std::to_string(i) + (jpeg ? ".jpg" : ".png");
            imageNames.push_back(imageName);
            result.Images.push_back({imageName, std::move(data)});
        }

    std::vector<std::string> materialNames;
    if (root.contains("materials"))
        for (std::size_t i = 0; i < root["materials"].size(); ++i) {
            const Json& m = root["materials"][i];
            const std::string key = UniqueIn(takenMaterials, m.value("name", "material" + std::to_string(i)));
            materialNames.push_back(key);
            ModelFile::Material& material = file.Materials[key];
            const Json pbr = m.value("pbrMetallicRoughness", Json::object());
            if (pbr.contains("baseColorFactor") && pbr["baseColorFactor"].size() == 4) {
                const Json& c = pbr["baseColorFactor"];
                material.Tint = {ToSrgb(c[0].get<float>()), ToSrgb(c[1].get<float>()), ToSrgb(c[2].get<float>()),
                                 c[3].get<float>()};
            }
            // A texture's image name (and the material's filter from its sampler), or "".
            bool secondUvs = false;
            auto textureOf = [&](const Json& owner, const char* slot) -> std::string {
                if (!owner.contains(slot) || !owner[slot].is_object()) return {};
                const Json& info = owner[slot];
                if (info.value("texCoord", 0) != 0) secondUvs = true;
                const std::size_t texture = info.value("index", std::size_t(0));
                if (!root.contains("textures") || texture >= root["textures"].size()) return {};
                const Json& t = root["textures"][texture];
                if (t.contains("sampler") && root.contains("samplers")) {
                    const Json& s = root["samplers"][t["sampler"].get<std::size_t>()];
                    if (s.value("magFilter", 9729) == 9729) material.TextureFilter = Filter::Linear;
                } else {
                    material.TextureFilter = Filter::Linear; // glTF's default sampler is smooth
                }
                const std::size_t source = t.value("source", std::size_t(0));
                return source < imageNames.size() ? imageNames[source] : std::string();
            };
            material.Texture = textureOf(pbr, "baseColorTexture");
            material.Metallic = std::clamp(pbr.value("metallicFactor", 1.0f), 0.0f, 1.0f);
            material.Roughness = std::clamp(pbr.value("roughnessFactor", 1.0f), 0.0f, 1.0f);
            material.MetalRough = textureOf(pbr, "metallicRoughnessTexture");
            material.Normal = textureOf(m, "normalTexture");
            if (!material.Normal.empty()) material.NormalScale = m["normalTexture"].value("scale", 1.0f);
            material.Occlusion = textureOf(m, "occlusionTexture");
            if (!material.Occlusion.empty())
                material.OcclusionStrength = std::clamp(m["occlusionTexture"].value("strength", 1.0f), 0.0f, 1.0f);
            material.EmissionTexture = textureOf(m, "emissiveTexture");
            if (secondUvs) notes.push_back("material " + key + " uses a second set of UVs: the first is used");
            const std::string alpha = m.value("alphaMode", std::string("OPAQUE"));
            if (alpha == "MASK") material.AlphaCutoff = std::clamp(m.value("alphaCutoff", 0.5f), 0.001f, 1.0f);
            if (alpha == "OPAQUE") material.Tint.A = 1;
            material.DoubleSided = m.value("doubleSided", false);
            if (m.contains("emissiveFactor") && m["emissiveFactor"].size() == 3) {
                const Json& e = m["emissiveFactor"];
                float strength = 1;
                if (m.contains("extensions") && m["extensions"].contains("KHR_materials_emissive_strength"))
                    strength = m["extensions"]["KHR_materials_emissive_strength"].value("emissiveStrength", 1.0f);
                material.Emission = std::max({e[0].get<float>(), e[1].get<float>(), e[2].get<float>()}) * strength;
            }
            if (!material.EmissionTexture.empty() && !m.contains("emissiveFactor"))
                notes.push_back("material " + key + ": an emission map without emissiveFactor does not glow");
            if (m.contains("extensions") && m["extensions"].contains("KHR_materials_unlit")) material.Unlit = true;
        }

    // ---- meshes: one per triangle primitive
    std::set<std::string> takenMeshes;
    std::vector<std::vector<std::string>> meshParts;
    if (root.contains("meshes"))
        for (std::size_t i = 0; i < root["meshes"].size(); ++i) {
            const Json& m = root["meshes"][i];
            const std::string base = m.value("name", "mesh" + std::to_string(i));
            std::vector<std::string> parts;
            const Json primitives = m.value("primitives", Json::array());
            for (std::size_t p = 0; p < primitives.size(); ++p) {
                const Json& prim = primitives[p];
                if (prim.value("mode", 4) != 4) {
                    notes.push_back("mesh " + base +
                                    ": only triangles are read (a primitive of lines or points was left out)");
                    continue;
                }
                if (prim.contains("targets")) notes.push_back("mesh " + base + ": morph targets are left out");
                const Json attributes = prim.value("attributes", Json::object());
                if (!attributes.contains("POSITION")) continue;
                int n = 0;
                const std::vector<float> positions = r.Floats(attributes["POSITION"].get<int>(), n);
                if (n != 3 || positions.empty()) continue;
                ModelFile::Mesh mesh;
                mesh.Vertices.resize(positions.size() / 3);
                for (std::size_t v = 0; v < mesh.Vertices.size(); ++v)
                    mesh.Vertices[v].Position = {positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
                if (attributes.contains("NORMAL")) {
                    const std::vector<float> normals = r.Floats(attributes["NORMAL"].get<int>(), n);
                    if (n == 3 && normals.size() == positions.size())
                        for (std::size_t v = 0; v < mesh.Vertices.size(); ++v)
                            mesh.Vertices[v].Normal = {normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2]};
                }
                if (attributes.contains("TEXCOORD_0")) {
                    const std::vector<float> uvs = r.Floats(attributes["TEXCOORD_0"].get<int>(), n);
                    if (n == 2 && uvs.size() == mesh.Vertices.size() * 2)
                        for (std::size_t v = 0; v < mesh.Vertices.size(); ++v)
                            mesh.Vertices[v].Uv = {uvs[v * 2], uvs[v * 2 + 1]};
                }
                if (attributes.contains("COLOR_0")) {
                    const std::vector<float> colors = r.Floats(attributes["COLOR_0"].get<int>(), n);
                    if ((n == 3 || n == 4) && colors.size() == mesh.Vertices.size() * std::size_t(n))
                        for (std::size_t v = 0; v < mesh.Vertices.size(); ++v) {
                            const float* c = &colors[v * std::size_t(n)];
                            mesh.Vertices[v].Tint = {ToSrgb(c[0]), ToSrgb(c[1]), ToSrgb(c[2]), n == 4 ? c[3] : 1.0f};
                        }
                }
                if (prim.contains("indices"))
                    mesh.Indices = r.Indices(prim["indices"].get<int>());
                else
                    Index(mesh);
                if (prim.contains("material") && prim["material"].get<std::size_t>() < materialNames.size())
                    mesh.Material = materialNames[prim["material"].get<std::size_t>()];
                // With some normals missing the rest would be dark: make them flat.
                const bool some = std::any_of(mesh.Vertices.begin(), mesh.Vertices.end(),
                                              [](const Vertex3D& v) { return v.Normal != Vec3{}; });
                const bool all = std::all_of(mesh.Vertices.begin(), mesh.Vertices.end(),
                                             [](const Vertex3D& v) { return v.Normal != Vec3{}; });
                if (some && !all) FlatNormals(mesh);
                const std::string key =
                    UniqueIn(takenMeshes, primitives.size() > 1 ? base + "." + std::to_string(p) : base);
                file.Meshes[key] = std::move(mesh);
                parts.push_back(key);
            }
            meshParts.push_back(std::move(parts));
        }

    // ---- nodes, parents first
    const std::size_t nodeCount = root.contains("nodes") ? root["nodes"].size() : 0;
    std::vector<int> parentOf(nodeCount, -1);
    for (std::size_t i = 0; i < nodeCount; ++i)
        for (const Json& c : root["nodes"][i].value("children", Json::array()))
            if (c.get<std::size_t>() < nodeCount) parentOf[c.get<std::size_t>()] = int(i);
    std::vector<std::size_t> roots;
    const std::size_t scene = root.value("scene", std::size_t(0));
    if (root.contains("scenes") && scene < root["scenes"].size()) {
        for (const Json& n : root["scenes"][scene].value("nodes", Json::array()))
            if (n.get<std::size_t>() < nodeCount) roots.push_back(n.get<std::size_t>());
    } else {
        for (std::size_t i = 0; i < nodeCount; ++i)
            if (parentOf[i] < 0) roots.push_back(i);
    }
    std::set<std::string> takenNodes;
    std::vector<std::string> nodeNames(nodeCount);
    std::vector<bool> visited(nodeCount, false);
    bool skins = false, cameras = false;
    std::function<void(std::size_t, int)> add = [&](std::size_t index, int parent) {
        if (visited[index]) return;
        visited[index] = true;
        const Json& n = root["nodes"][index];
        ModelFile::Node node;
        node.Name = UniqueIn(takenNodes, n.value("name", "node" + std::to_string(index)));
        nodeNames[index] = node.Name;
        node.Parent = parent;
        if (n.contains("matrix") && n["matrix"].size() == 16) {
            Decompose(n["matrix"], node.Position, node.Rotation, node.Scale);
        } else {
            node.Position = Vec3Of(n.value("translation", Json()), {0, 0, 0});
            node.Scale = Vec3Of(n.value("scale", Json()), {1, 1, 1});
            if (n.contains("rotation") && n["rotation"].size() == 4) {
                const Json& q = n["rotation"];
                node.Rotation =
                    Quat{q[0].get<float>(), q[1].get<float>(), q[2].get<float>(), q[3].get<float>()}.Normalized();
            }
        }
        skins = skins || n.contains("skin");
        cameras = cameras || n.contains("camera");
        std::vector<std::string> parts;
        if (n.contains("mesh") && n["mesh"].get<std::size_t>() < meshParts.size())
            parts = meshParts[n["mesh"].get<std::size_t>()];
        if (!parts.empty()) node.Mesh = parts[0];
        const int self = int(file.Nodes.size());
        file.Nodes.push_back(node);
        for (std::size_t p = 1; p < parts.size(); ++p) {
            ModelFile::Node extra;
            extra.Name = UniqueIn(takenNodes, node.Name + "." + std::to_string(p));
            extra.Parent = self;
            extra.Mesh = parts[p];
            file.Nodes.push_back(extra);
        }
        for (const Json& c : n.value("children", Json::array()))
            if (c.get<std::size_t>() < nodeCount) add(c.get<std::size_t>(), self);
    };
    for (const std::size_t i : roots) add(i, -1);
    if (skins) notes.push_back("skins (bones) are left out: the meshes keep their rest shape");
    if (cameras) notes.push_back("cameras are left out");
    // Meshes that no node of the scene draws still come in, on a node of their own.
    if (nodeCount == 0)
        for (const auto& [key, mesh] : file.Meshes) {
            ModelFile::Node node;
            node.Name = UniqueIn(takenNodes, key);
            node.Mesh = key;
            file.Nodes.push_back(node);
        }

    // ---- animations
    std::set<std::string> takenAnimations;
    std::vector<std::string> animationNames;
    if (root.contains("animations"))
        for (std::size_t i = 0; i < root["animations"].size(); ++i) {
            const Json& a = root["animations"][i];
            const std::string key = UniqueIn(takenAnimations, a.value("name", "animation" + std::to_string(i)));
            animationNames.push_back(key);
            ModelFile::Animation& animation = file.Animations[key];
            const Json samplers = a.value("samplers", Json::array());
            for (const Json& c : a.value("channels", Json::array())) {
                const Json target = c.value("target", Json::object());
                const std::string path = target.value("path", std::string());
                const std::size_t node = target.value("node", nodeCount);
                const std::size_t sampler = c.value("sampler", samplers.size());
                if (node >= nodeCount || nodeNames[node].empty() || sampler >= samplers.size()) continue;
                ModelFile::Track track;
                track.Node = nodeNames[node];
                if (path == "translation")
                    track.Property = ModelFile::Property::Position;
                else if (path == "rotation")
                    track.Property = ModelFile::Property::Rotation;
                else if (path == "scale")
                    track.Property = ModelFile::Property::Scale;
                else {
                    notes.push_back("animation " + key + ": " + path + " tracks are left out");
                    continue;
                }
                const Json& s = samplers[sampler];
                const std::string interpolation = s.value("interpolation", std::string("LINEAR"));
                int n = 0;
                track.Times = r.Floats(s.value("input", -1), n);
                std::vector<float> values = r.Floats(s.value("output", -1), n);
                const std::size_t width = track.Property == ModelFile::Property::Rotation ? 4 : 3;
                if (interpolation == "CUBICSPLINE") {
                    notes.push_back("animation " + key + ": cubic curves become straight lines between their keys");
                    std::vector<float> middle;
                    for (std::size_t k = 0; k + 3 * width <= values.size(); k += 3 * width)
                        middle.insert(middle.end(), values.begin() + std::ptrdiff_t(k + width),
                                      values.begin() + std::ptrdiff_t(k + 2 * width));
                    values = std::move(middle);
                }
                track.Step = interpolation == "STEP";
                track.Values = std::move(values);
                if (track.Values.size() != track.Times.size() * width) continue;
                animation.Tracks.push_back(std::move(track));
            }
        }

    // ---- what only BitMotor has, from ExportGlb
    if (root.contains("extras") && root["extras"].contains("BitMotor")) {
        const Json& extra = root["extras"]["BitMotor"];
        const Json animations = extra.value("Animations", Json::object()),
                   sockets = extra.value("Sockets", Json::object());
        for (auto& [key, a] : animations.items()) {
            ModelFile::Animation& animation = file.Animations[key];
            if (a.contains("Duration") && a["Duration"].is_number()) animation.Duration = a["Duration"].get<float>();
            animation.Loop = a.value("Loop", true);
            const Json events = a.value("Events", Json::object());
            for (auto& [event, times] : events.items())
                for (const Json& t : times) animation.Events[event].push_back(t.get<float>());
        }
        for (auto& [key, s] : sockets.items()) {
            ModelFile::Socket socket;
            socket.Node = s.value("Node", std::string());
            socket.Position = Vec3Of(s.value("Position", Json()), {0, 0, 0});
            if (s.contains("Rotation") && s["Rotation"].size() == 4) {
                const Json& q = s["Rotation"];
                socket.Rotation =
                    Quat{q[0].get<float>(), q[1].get<float>(), q[2].get<float>(), q[3].get<float>()}.Normalized();
            }
            file.Sockets[key] = socket;
        }
    }
    std::sort(notes.begin(), notes.end());
    notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
    return result;
}

// ---------------------------------------------------------------- export

namespace {

struct Writer {
    Json Root = Json::object();
    Bytes Bin;

    // Bytes into the buffer (aligned to 4) as a bufferView; returns its index.
    std::size_t View(const void* data, std::size_t size, int target) {
        while (Bin.size() % 4) Bin.push_back(0);
        const std::size_t offset = Bin.size();
        const auto* p = static_cast<const std::uint8_t*>(data);
        Bin.insert(Bin.end(), p, p + size);
        Json view = {{"buffer", 0}, {"byteOffset", offset}, {"byteLength", size}};
        if (target) view["target"] = target;
        Root["bufferViews"].push_back(view);
        return Root["bufferViews"].size() - 1;
    }

    std::size_t Accessor(const std::vector<float>& values, int components, const char* type, int target, bool bounds) {
        const std::size_t view = View(values.data(), values.size() * sizeof(float), target);
        Json a = {{"bufferView", view},
                  {"componentType", 5126},
                  {"count", values.size() / std::size_t(components)},
                  {"type", type}};
        if (bounds && !values.empty()) {
            std::vector<float> lo(values.begin(), values.begin() + components), hi = lo;
            for (std::size_t i = 0; i < values.size(); ++i) {
                const std::size_t k = i % std::size_t(components);
                lo[k] = std::min(lo[k], values[i]);
                hi[k] = std::max(hi[k], values[i]);
            }
            a["min"] = lo;
            a["max"] = hi;
        }
        Root["accessors"].push_back(a);
        return Root["accessors"].size() - 1;
    }

    std::size_t Indices(const std::vector<std::uint32_t>& values) {
        const std::size_t view = View(values.data(), values.size() * 4, 34963);
        Root["accessors"].push_back(
            {{"bufferView", view}, {"componentType", 5125}, {"count", values.size()}, {"type", "SCALAR"}});
        return Root["accessors"].size() - 1;
    }
};

std::vector<float> Floats(std::initializer_list<float> v) { return v; }

} // namespace

std::vector<std::uint8_t> ExportGlb(const ModelFile& file) {
    // The buffer is written little-endian, as glTF wants (floats copied as they are in memory).
    Writer w;
    w.Root["asset"] = {{"version", "2.0"}, {"generator", "BitMotor"}};
    std::set<std::string> extensions;

    // Materials, with their textures by path.
    std::map<std::string, std::size_t> materialIndex, imageIndex;
    std::map<std::pair<std::string, int>, std::size_t> textureIndex;
    for (const auto& [key, m] : file.Materials) {
        Json material = {{"name", key}};
        Json pbr = {{"baseColorFactor", Floats({ToLinear(m.Tint.R), ToLinear(m.Tint.G), ToLinear(m.Tint.B), m.Tint.A})},
                    {"metallicFactor", m.Metallic},
                    {"roughnessFactor", m.Roughness}};
        // The texture for an image (one per image and filter), by index.
        auto texture = [&](const std::string& image) {
            if (!imageIndex.contains(image)) {
                w.Root["images"].push_back({{"uri", image}});
                imageIndex[image] = w.Root["images"].size() - 1;
            }
            const int filter = m.TextureFilter == Filter::Linear ? 9729 : 9728;
            const auto texKey = std::pair{image, filter};
            if (!textureIndex.contains(texKey)) {
                if (!w.Root.contains("samplers")) w.Root["samplers"] = Json::array();
                w.Root["samplers"].push_back({{"magFilter", filter}, {"minFilter", filter}});
                w.Root["textures"].push_back(
                    {{"source", imageIndex[image]}, {"sampler", w.Root["samplers"].size() - 1}});
                textureIndex[texKey] = w.Root["textures"].size() - 1;
            }
            return textureIndex[texKey];
        };
        if (!m.Texture.empty()) pbr["baseColorTexture"] = {{"index", texture(m.Texture)}};
        if (!m.MetalRough.empty()) pbr["metallicRoughnessTexture"] = {{"index", texture(m.MetalRough)}};
        material["pbrMetallicRoughness"] = pbr;
        if (!m.Normal.empty()) {
            material["normalTexture"] = {{"index", texture(m.Normal)}};
            if (m.NormalScale != 1) material["normalTexture"]["scale"] = m.NormalScale;
        }
        if (!m.Occlusion.empty()) {
            material["occlusionTexture"] = {{"index", texture(m.Occlusion)}};
            if (m.OcclusionStrength != 1) material["occlusionTexture"]["strength"] = m.OcclusionStrength;
        }
        if (!m.EmissionTexture.empty()) material["emissiveTexture"] = {{"index", texture(m.EmissionTexture)}};
        if (m.AlphaCutoff > 0) {
            material["alphaMode"] = "MASK";
            material["alphaCutoff"] = m.AlphaCutoff;
        }
        if (m.DoubleSided) material["doubleSided"] = true;
        if (m.Emission > 0) {
            const float e = std::min(m.Emission, 1.0f);
            material["emissiveFactor"] = Floats({e, e, e});
            if (m.Emission > 1) {
                material["extensions"]["KHR_materials_emissive_strength"] = {{"emissiveStrength", m.Emission}};
                extensions.insert("KHR_materials_emissive_strength");
            }
        }
        if (m.Unlit) {
            material["extensions"]["KHR_materials_unlit"] = Json::object();
            extensions.insert("KHR_materials_unlit");
        }
        if (m.Tint.A < 1 && m.AlphaCutoff <= 0) material["alphaMode"] = "BLEND";
        w.Root["materials"].push_back(material);
        materialIndex[key] = w.Root["materials"].size() - 1;
    }

    // Meshes: one primitive each.
    std::map<std::string, std::size_t> meshIndex;
    for (const auto& [key, original] : file.Meshes) {
        if (original.Vertices.empty()) continue;
        ModelFile::Mesh mesh = original;
        Index(mesh);
        if (mesh.Indices.empty()) continue;
        if (!std::all_of(mesh.Vertices.begin(), mesh.Vertices.end(),
                         [](const Vertex3D& v) { return v.Normal != Vec3{}; }))
            FlatNormals(mesh);
        std::vector<float> positions, normals, uvs, colors;
        bool anyUv = false, anyColor = false;
        for (const Vertex3D& v : mesh.Vertices) {
            positions.insert(positions.end(), {v.Position.X, v.Position.Y, v.Position.Z});
            normals.insert(normals.end(), {v.Normal.X, v.Normal.Y, v.Normal.Z});
            uvs.insert(uvs.end(), {v.Uv.X, v.Uv.Y});
            colors.insert(colors.end(), {ToLinear(v.Tint.R), ToLinear(v.Tint.G), ToLinear(v.Tint.B), v.Tint.A});
            anyUv = anyUv || v.Uv != Vec2{};
            anyColor = anyColor || v.Tint != Colors::White;
        }
        Json attributes = {{"POSITION", w.Accessor(positions, 3, "VEC3", 34962, true)},
                           {"NORMAL", w.Accessor(normals, 3, "VEC3", 34962, false)}};
        if (anyUv) attributes["TEXCOORD_0"] = w.Accessor(uvs, 2, "VEC2", 34962, false);
        if (anyColor) attributes["COLOR_0"] = w.Accessor(colors, 4, "VEC4", 34962, false);
        Json primitive = {{"attributes", attributes}, {"indices", w.Indices(mesh.Indices)}, {"mode", 4}};
        if (auto found = materialIndex.find(mesh.Material); found != materialIndex.end())
            primitive["material"] = found->second;
        w.Root["meshes"].push_back({{"name", key}, {"primitives", Json::array({primitive})}});
        meshIndex[key] = w.Root["meshes"].size() - 1;
    }

    // Nodes and the scene.
    Json scene = Json::array();
    for (std::size_t i = 0; i < file.Nodes.size(); ++i) {
        const ModelFile::Node& n = file.Nodes[i];
        Json node = {{"name", n.Name}};
        if (auto found = meshIndex.find(n.Mesh); found != meshIndex.end()) node["mesh"] = found->second;
        if (n.Position != Vec3{}) node["translation"] = Floats({n.Position.X, n.Position.Y, n.Position.Z});
        if (n.Rotation != Quat{}) {
            const Quat q = n.Rotation.Normalized();
            node["rotation"] = Floats({q.X, q.Y, q.Z, q.W});
        }
        if (n.Scale != Vec3{1, 1, 1}) node["scale"] = Floats({n.Scale.X, n.Scale.Y, n.Scale.Z});
        Json children = Json::array();
        for (std::size_t c = i + 1; c < file.Nodes.size(); ++c)
            if (file.Nodes[c].Parent == int(i)) children.push_back(c);
        if (!children.empty()) node["children"] = children;
        w.Root["nodes"].push_back(node);
        if (n.Parent < 0 || n.Parent >= int(i)) scene.push_back(i);
    }
    w.Root["scenes"] = Json::array({{{"nodes", scene}}});
    w.Root["scene"] = 0;

    // Animations, and what glTF has no place for in the extras.
    Json extra = Json::object();
    for (const auto& [key, a] : file.Animations) {
        Json samplers = Json::array(), channels = Json::array();
        for (const ModelFile::Track& t : a.Tracks) {
            const int node = file.FindNode(t.Node);
            const std::size_t width = t.Property == ModelFile::Property::Rotation ? 4 : 3;
            if (node < 0 || t.Times.empty() || t.Values.size() != t.Times.size() * width) continue;
            const std::size_t input = w.Accessor(t.Times, 1, "SCALAR", 0, true);
            const std::size_t output = w.Accessor(t.Values, int(width), width == 4 ? "VEC4" : "VEC3", 0, false);
            samplers.push_back({{"input", input}, {"output", output}, {"interpolation", t.Step ? "STEP" : "LINEAR"}});
            const char* path = t.Property == ModelFile::Property::Position   ? "translation"
                               : t.Property == ModelFile::Property::Rotation ? "rotation"
                                                                             : "scale";
            channels.push_back({{"sampler", samplers.size() - 1}, {"target", {{"node", node}, {"path", path}}}});
        }
        if (!channels.empty())
            w.Root["animations"].push_back({{"name", key}, {"samplers", samplers}, {"channels", channels}});
        Json info = Json::object();
        if (a.Duration) info["Duration"] = *a.Duration;
        if (!a.Loop) info["Loop"] = false;
        if (!a.Events.empty()) info["Events"] = a.Events;
        extra["Animations"][key] = info;
    }
    for (const auto& [key, s] : file.Sockets)
        extra["Sockets"][key] = {{"Node", s.Node},
                                 {"Position", Floats({s.Position.X, s.Position.Y, s.Position.Z})},
                                 {"Rotation", Floats({s.Rotation.X, s.Rotation.Y, s.Rotation.Z, s.Rotation.W})}};
    if (!extra.empty()) w.Root["extras"]["BitMotor"] = extra;
    if (!extensions.empty())
        w.Root["extensionsUsed"] = Json(std::vector<std::string>(extensions.begin(), extensions.end()));

    while (w.Bin.size() % 4) w.Bin.push_back(0);
    if (!w.Bin.empty()) w.Root["buffers"] = Json::array({{{"byteLength", w.Bin.size()}}});
    std::string json = w.Root.dump();
    while (json.size() % 4) json += ' ';

    Bytes out;
    WriteU32(out, GlbMagic);
    WriteU32(out, 2);
    WriteU32(out, std::uint32_t(12 + 8 + json.size() + (w.Bin.empty() ? 0 : 8 + w.Bin.size())));
    WriteU32(out, std::uint32_t(json.size()));
    WriteU32(out, JsonChunk);
    out.insert(out.end(), json.begin(), json.end());
    if (!w.Bin.empty()) {
        WriteU32(out, std::uint32_t(w.Bin.size()));
        WriteU32(out, BinChunk);
        out.insert(out.end(), w.Bin.begin(), w.Bin.end());
    }
    return out;
}

} // namespace BitMotor::Modeling
