#pragma once
#include "BitMotor/Render/Model.h"
#include "BitMotor/Render/ModelFile.h"

#include <filesystem>
#include <string_view>

#include <map>
#include <memory>
#include <string>
#include <vector>

struct SDL_GPUBuffer;
struct SDL_GPUCopyPass;
struct SDL_GPUCommandBuffer;
struct SDL_GPUTexture;

namespace BitMotor::Detail {

struct MeshData {
    std::vector<Vertex3D> Vertices;
    std::vector<std::uint32_t> Indices;
    Vec3 Min, Max;
    // On the GPU once it has been drawn.
    SDL_GPUBuffer* VertexBuffer = nullptr;
    SDL_GPUBuffer* IndexBuffer = nullptr;
    MeshData();
    ~MeshData();
    MeshData(const MeshData&) = delete;
    MeshData& operator=(const MeshData&) = delete;
};

struct ModelNode {
    std::string Name;
    int Parent = -1;
    int Mesh = -1; // index into ModelData::Meshes
    Vec3 Position;
    Quat Rotation;
    Vec3 Scale = {1, 1, 1};
};

struct ModelPart {
    BitMotor::Mesh Geometry;
    Material Look;
};

enum class TrackProperty { Position, Rotation, Scale };

struct ModelTrack {
    int Node = 0;
    TrackProperty Property = TrackProperty::Position;
    bool Step = false;
    std::vector<float> Times;
    std::vector<float> Values; // 3 per time (4 for rotations)
};

struct ModelAnimationData {
    float Duration = 0;
    bool Loop = true;
    std::vector<ModelTrack> Tracks;
    std::vector<std::pair<std::string, float>> Events;
};

struct ModelSocket {
    int Node = 0;
    Vec3 Position;
    Quat Rotation;
};

struct ModelData {
    std::vector<ModelPart> Meshes;
    std::vector<ModelNode> Nodes; // parents before their children
    std::map<std::string, ModelAnimationData, std::less<>> Animations;
    std::map<std::string, ModelSocket, std::less<>> Sockets;
};

// What Screen::Draw(Model3D) queues for the frame: one mesh with its place.
struct ModelDraw {
    std::shared_ptr<MeshData> Geometry;
    Material Look;
    Mat4 World;
    Color Tint;
};

// A ModelFile ready to draw (ModelFile.cpp): names resolved (what points nowhere is warned about and left
// out), rotations normalised and the textures loaded. Not `full`: only the nodes, animations and sockets, and
// quiet (for poses).
std::shared_ptr<ModelData> BuildModel(const ModelFile& file, const std::filesystem::path& folder, std::string_view name,
                                      bool full);
// A number as .bitmodel files write it: at most 6 decimals, no trailing zeros, never "-0".
std::string ModelNumber(float value);
// Where every node is at `seconds` into `animation` (Model.cpp).
std::vector<Mat4> ComputePose(const ModelData& data, std::string_view animation, float seconds);

std::vector<ModelDraw>& ModelQueue();
std::vector<PointLight3D>& PointLightQueue();
// The start of a frame (and Screen::Clear): nothing queued.
void ResetModels();
// The frame's Line3D and Point3D as camera-facing quads for `camera` (nullptr when there are none).
std::shared_ptr<MeshData> OverlayMesh(const Camera3D& camera);
// Queues them as one unlit model (before the models go to the GPU).
void QueueOverlay();

// The GPU side (Renderer3D.cpp). Start and Stop follow the renderer's device.
bool StartModels();
void StopModels();
// New meshes go up in the frame's copy pass, before any render pass.
void UploadModels(SDL_GPUCopyPass* copy);
// The queued models into `scene` (HDR, `width` x `height`), on what it already holds, with their own depth.
void DrawModels(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* scene, unsigned width, unsigned height);
// Every mesh lets its GPU buffers go (the device stops).
void ReleaseMeshes();

} // namespace BitMotor::Detail
