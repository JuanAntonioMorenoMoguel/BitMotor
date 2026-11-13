#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Matrix.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Model.h"
#include "BitMotor/Render/Texture.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A .bitmodel as it is written: every material, mesh, node, animation and socket by its name, with the paths
// of the textures. It is what a model editor (BitModel) or a tool edits, saves and turns into a Model to draw.
// A game that only draws models uses Model::Load and never needs it.
//     auto file = ModelFile::Load("models/robot.bitmodel");
//     file->Meshes["box"] = Modeling::Cube({1, 1, 1});
//     file->Nodes.push_back(ModelFile::Node{.Name = "box", .Mesh = "box", .Position = {0, 0.5f, 0}});
//     file->Save("robot.bitmodel");                     // keys in a fixed order, at most 6 decimals
//     Model robot = Model::FromFile(*file);
// The format is in docs/BitModel.md. Modeling.h has the functions that build and change it.
namespace BitMotor {

struct ModelFile {
    struct Material {
        // Multiplies the texture and the vertex colours ("Color" in the file).
        BitMotor::Color Tint = Colors::White;
        // PNG relative to the .bitmodel; empty, no texture.
        std::string Texture;
        BitMotor::Filter TextureFilter = BitMotor::Filter::Nearest;
        float Emission = 0;
        bool Unlit = false;
        // Version 2 (BitMotor::Material has what each one does). The maps are PNGs next to the .bitmodel,
        // with the same TextureFilter.
        float Metallic = 0;
        float Roughness = 1;
        std::string Normal;
        float NormalScale = 1;
        std::string MetalRough;
        std::string Occlusion;
        float OcclusionStrength = 1;
        std::string EmissionTexture;
        float AlphaCutoff = 0;
        bool DoubleSided = false;
        // Uses something of version 2.
        bool Physical() const;
        bool operator==(const Material&) const = default;
    };

    struct Mesh {
        std::string Material;
        // Normals all zero: flat normals when it is drawn. Uvs all (0, 0) and colours all white are not
        // written.
        std::vector<Vertex3D> Vertices;
        // Three per triangle, counter-clockwise seen from outside. Empty: the vertices three by three.
        std::vector<std::uint32_t> Indices;
        bool operator==(const Mesh&) const;
    };

    struct Node {
        // Unique inside the model: animations and sockets find nodes by name.
        std::string Name;
        // Index in Nodes, before this one; -1: a root.
        int Parent = -1;
        // Empty: it only moves its children.
        std::string Mesh;
        Vec3 Position = {0, 0, 0};
        Quat Rotation = {};
        Vec3 Scale = {1, 1, 1};
        bool operator==(const Node&) const = default;
    };

    enum class Property { Position, Rotation, Scale };

    struct Track {
        std::string Node;
        ModelFile::Property Property = ModelFile::Property::Position;
        // "Step" in the file: holds each value until the next time. false: linear (slerp for rotations).
        bool Step = false;
        // Seconds, growing.
        std::vector<float> Times;
        // 3 per time (4 for Rotation: a quaternion X, Y, Z, W).
        std::vector<float> Values;
        bool operator==(const Track&) const = default;
    };

    struct Animation {
        // nullopt: the last time of its tracks.
        std::optional<float> Duration;
        bool Loop = true;
        std::vector<Track> Tracks;
        // Event name: the seconds where it happens.
        std::map<std::string, std::vector<float>> Events;
        // Duration, or the last time of its tracks.
        float Length() const;
        bool operator==(const Animation&) const = default;
    };

    struct Socket {
        std::string Node;
        Vec3 Position = {0, 0, 0};
        Quat Rotation = {};
        bool operator==(const Socket&) const = default;
    };

    std::string Format = "bitmodel";
    int Version = 1;
    std::map<std::string, Material> Materials;
    std::map<std::string, Mesh> Meshes;
    // Parents before their children.
    std::vector<Node> Nodes;
    std::map<std::string, Animation> Animations;
    std::map<std::string, Socket> Sockets;

    // Reads a .bitmodel (relative paths: Files::AssetPath). nullopt if it cannot be read or it is not a
    // bitmodel (logged). Unknown keys and values of the wrong type are warned about and skipped; names that
    // point nowhere are kept as they are (Model::FromFile warns about them, Modeling::Check lists them).
    static std::optional<ModelFile> Load(const std::filesystem::path& path);
    static std::optional<ModelFile> Parse(std::string_view json, std::string_view name = "model");
    // The JSON text: keys in the order of docs/BitModel.md, names sorted, numbers with at most 6 decimals,
    // one vertex (or triangle) per line. The same model always gives the same bytes.
    std::string ToJson() const;
    // Writes ToJson (relative paths: Files::SavePath).
    bool Save(const std::filesystem::path& path) const;

    // The index of the node with that name in Nodes, or -1.
    int FindNode(std::string_view name) const;

    bool operator==(const ModelFile&) const = default;
};

} // namespace BitMotor
