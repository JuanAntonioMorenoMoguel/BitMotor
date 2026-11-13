#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Math/Matrix.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/ModelFile.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Building and changing models as data (ModelFile): primitives, mesh edits, names, keys of animations,
// checks, and OBJ files. Everything here is plain functions on plain data, with no window or GPU, so a
// model editor (BitModel), a tool or a game that makes models by code use the same steps, and the same input
// always gives the same result.
//     ModelFile file;
//     file.Meshes["tower"] = Modeling::Cylinder(0.5f, 2, 8);
//     auto top = Modeling::FacesFacing(file.Meshes["tower"], {0, 1, 0});
//     top = Modeling::Inset(file.Meshes["tower"], top, 0.3f);
//     Modeling::Extrude(file.Meshes["tower"], top, -0.2f);           // a hollow on top
//     Modeling::Paint(file.Meshes["tower"], top, Colors::Slate);
//     file.Nodes.push_back(ModelFile::Node{.Name = "tower", .Mesh = "tower"});
//     Modeling::SetKey(file, "idle", "tower", ModelFile::Property::Scale, 0.5f, std::vector<float>{1, 1.1f, 1});
// Faces are triangles, numbered by their place in Indices (face f is Indices[3f], [3f+1], [3f+2]). A mesh
// with no Indices is given them by the first edit. Edits that need a face to have vertices of its own (paint,
// UVs, flipping) copy its vertices first, so neighbouring faces keep theirs.
namespace BitMotor::Modeling {

using Faces = std::vector<std::uint32_t>;

// ---------------------------------------------------------------- primitives (centred on the origin)

ModelFile::Mesh Cube(Vec3 size = {1, 1, 1});
ModelFile::Mesh Sphere(float radius = 0.5f, int segments = 16);
// Lying flat, facing +Y. `cells` squares along each side.
ModelFile::Mesh Plane(Vec2 size = {1, 1}, int cells = 1);
// Standing on Y, from -height/2 to +height/2.
ModelFile::Mesh Cylinder(float radius = 0.5f, float height = 1, int segments = 16);
// Its tip at +height/2.
ModelFile::Mesh Cone(float radius = 0.5f, float height = 1, int segments = 16);
// A mesh that is already on the GPU side, and back.
ModelFile::Mesh FromMesh(const BitMotor::Mesh& mesh);
BitMotor::Mesh ToMesh(const ModelFile::Mesh& mesh);

// ---------------------------------------------------------------- looking at a mesh

int FaceCount(const ModelFile::Mesh& mesh);
// Every face.
Faces AllFaces(const ModelFile::Mesh& mesh);
// The faces whose normal is within `degrees` of `direction`.
Faces FacesFacing(const ModelFile::Mesh& mesh, Vec3 direction, float degrees = 1);
Vec3 FaceNormal(const ModelFile::Mesh& mesh, std::uint32_t face);
Vec3 FaceCentre(const ModelFile::Mesh& mesh, std::uint32_t face);
// The vertices the faces use, sorted, each once.
std::vector<std::uint32_t> Corners(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces);
// `vertices` and every other vertex in the same place: what moves together when a corner is dragged.
std::vector<std::uint32_t> SamePlace(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> vertices);
// The faces that touch these faces through an edge or a corner, and so on until nothing more is reached: the
// piece they are part of (edges and corners are matched by position).
Faces Connected(const ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces);

struct Bounds {
    Vec3 Min, Max;
};
Bounds BoundsOf(const ModelFile::Mesh& mesh);

struct Hit {
    std::uint32_t Face = 0;
    float Distance = 0; // along `direction`, in units of its length
    Vec3 Point;
};
// The nearest face that a ray from `origin` towards `direction` meets, with the mesh placed by `world`.
std::optional<Hit> Raycast(const ModelFile::Mesh& mesh, const Mat4& world, Vec3 origin, Vec3 direction);

// ---------------------------------------------------------------- changing a mesh

// Gives a mesh with no Indices its indices, and drops triangles that point at vertices that do not exist.
void Index(ModelFile::Mesh& mesh);
// Vertices nobody uses go away (the rest keep their order).
void RemoveUnused(ModelFile::Mesh& mesh);
// Vertices equal in everything (position, normal, UV, colour) within `epsilon` become one.
void Weld(ModelFile::Mesh& mesh, float epsilon = 1e-5f);
// Moves, turns or scales vertices (normals follow).
void Transform(ModelFile::Mesh& mesh, std::span<const std::uint32_t> vertices, const Mat4& transform);
void TransformAll(ModelFile::Mesh& mesh, const Mat4& transform);
// One normal per face (the low-poly look).
void FlatNormals(ModelFile::Mesh& mesh);
// Normals shared by the vertices in the same place: rounded shading.
void SmoothNormals(ModelFile::Mesh& mesh);
// No normals in the file: BitMotor makes flat ones when it draws. The smallest files.
void ClearNormals(ModelFile::Mesh& mesh);
// The faces get vertices of their own (shared among themselves). The faces keep their numbers.
void Separate(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces);
// Turns the faces inside out.
void Flip(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces);
// Removes the faces (and the vertices only they used). Faces after them move down.
void Delete(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces);
// Pushes the faces out along their average normal by `distance` (negative: in) and closes the sides with new
// faces. Returns the faces that moved (their numbers do not change).
Faces Extrude(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, float distance);
// Each face gets a smaller copy of itself inside (`amount` 0..1 of the way to its centre) and a ring of faces
// around it. Returns the inner faces.
Faces Inset(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, float amount);
// Each face becomes four. Returns them all.
Faces Subdivide(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces);
// Adds a copy mirrored across the plane where `axis` (0 X, 1 Y, 2 Z) is 0, then welds the seam.
void Mirror(ModelFile::Mesh& mesh, int axis = 0);
// Adds another mesh, placed by `transform`. Returns its faces.
Faces Append(ModelFile::Mesh& mesh, const ModelFile::Mesh& other, const Mat4& transform = {});
// Colours the faces (vertex colours).
void Paint(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, Color color);
// Points the faces at one colour of a palette texture: `columns` x `rows` cells, `cell` counted left to
// right, top to bottom.
void PaletteUv(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, int cell, int columns, int rows = 1);
// UVs projected from the side each face looks at most: `scale` texture repeats per unit.
void BoxUv(ModelFile::Mesh& mesh, std::span<const std::uint32_t> faces, float scale = 1);

// ---------------------------------------------------------------- the model as a whole

enum class Part { Material, Mesh, Node, Animation, Socket };
bool Has(const ModelFile& file, Part part, std::string_view name);
// `base` if it is free, else base2, base3...
std::string UniqueName(const ModelFile& file, Part part, std::string_view base);
// Renames a part and everything that points at it (meshes' materials, nodes' meshes, tracks' and sockets'
// nodes). false if `from` does not exist or `to` is empty or taken.
bool Rename(ModelFile& file, Part part, std::string_view from, std::string_view to);
// Removes a node and what points at it (its tracks and sockets); its children go to its parent, keeping
// where they are when the scales allow it.
void RemoveNode(ModelFile& file, int node);
// Hangs `node` from `parent` (-1: a root), reordering Nodes so parents stay first. Its local position,
// rotation and scale stay as they are. false if `parent` is the node or one of its children.
bool SetParent(ModelFile& file, int node, int parent);
// Where every node is (in the model's space) at `seconds` into `animation` (empty: the rest pose), as
// Model::Pose would say, without making the model. Quiet about names that point nowhere.
std::vector<Mat4> Pose(const ModelFile& file, std::string_view animation = {}, float seconds = 0);
// A node's position, rotation or scale in an animation at `seconds` (its rest value where it has no track).
std::vector<float> ValueAt(const ModelFile& file, std::string_view animation, std::string_view node,
                           ModelFile::Property property, float seconds);
// Sets a key (3 values, 4 for a rotation) at `seconds`, making the animation and its track if needed and
// replacing a key at the same time. false if `values` has the wrong size.
bool SetKey(ModelFile& file, std::string_view animation, std::string_view node, ModelFile::Property property,
            float seconds, std::span<const float> values);
// Removes the key at `seconds` (the track goes when it has none). false if there was none.
bool RemoveKey(ModelFile& file, std::string_view animation, std::string_view node, ModelFile::Property property,
               float seconds);

// ---------------------------------------------------------------- checks

enum class Issue {
    MissingMaterial,  // a mesh names a material that does not exist
    MissingMesh,      // a node names a mesh that does not exist
    MissingNode,      // a track or a socket names a node that does not exist
    DuplicateNode,    // two nodes with the same name
    ParentAfterChild, // a node's parent comes after it (or does not exist)
    EmptyMesh,        // a mesh without vertices
    BadIndex,         // an index points at a vertex that does not exist, or Indices is not a multiple of 3
    DegenerateFace,   // a triangle with no area
    BadTrack,         // a track without times, or with the wrong number of values
    TimesNotGrowing,  // a track's times go back
    UnusedMesh,       // no node draws it
};
struct Problem {
    Modeling::Issue Issue;
    // Where: "mesh crate", "node lid", "animation open"...
    std::string Where;
};
// What would be wrong, or warned about, when the model is drawn. Empty: all good.
std::vector<Problem> Check(const ModelFile& file);

// ---------------------------------------------------------------- OBJ

// A Wavefront OBJ (with its MTL, if there is one) as a model: one mesh and one node for each object or
// group (split by material), vertex colours ("v x y z r g b"), UVs, normals, and the materials' Kd, d,
// map_Kd, Ke and illum 0 (unlit). Faces with more than three corners are split in fans. nullopt if it
// has no faces.
std::optional<ModelFile> ImportObj(std::string_view obj, std::string_view mtl = {}, std::string_view name = "obj");
struct ObjFiles {
    std::string Obj, Mtl;
};
// The model in the pose of `animation` at `seconds` as OBJ and MTL text. `mtlName` is the MTL's file name, for
// the OBJ's mtllib line.
ObjFiles ExportObj(const ModelFile& file, std::string_view mtlName = "model.mtl", std::string_view animation = {},
                   float seconds = 0);

// ---------------------------------------------------------------- glTF

struct GltfImage {
    // The file name the model's material uses ("robot_0.png"): write Bytes there, next to the .bitmodel.
    std::string Name;
    std::vector<std::uint8_t> Bytes;
};
struct GltfImport {
    ModelFile File;
    // Images that were inside the glTF (in a .glb or as data). Images it only names keep their uri as the
    // material's Texture and are not here.
    std::vector<GltfImage> Images;
    // What could not come across as it was (skins, morph targets, cubic curves...), for an import report.
    std::vector<std::string> Notes;
};
// Reads a file a .gltf names (an external .bin buffer) by its uri, or nullopt.
using GltfFiles = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view uri)>;
// A glTF 2.0 model (a .glb, or the text of a .gltf) as a model: its default scene's nodes with their
// position, rotation and scale (matrices are split into them), one mesh per triangle primitive with
// positions, normals, the first UVs and vertex colours, materials (base colour and its texture, emission,
// KHR_materials_unlit) and node animations (linear, step; cubic ones keep only their values). Skins and morph
// targets are left out and noted. Sockets and events come back from files that ExportGlb wrote. nullopt
// if it cannot be read.
std::optional<GltfImport> ImportGltf(std::span<const std::uint8_t> bytes, const GltfFiles& files = {},
                                     std::string_view name = "gltf");
// The model as a .glb: meshes, materials (textures by their path, not embedded), nodes, animations, and the
// sockets and events in the file's extras. The same model always gives the same bytes.
std::vector<std::uint8_t> ExportGlb(const ModelFile& file);

} // namespace BitMotor::Modeling
