#include "Printing.h"
#include "Testing.h"

#include <BitMotor/BitMotor.h>

#include <cmath>
#include <map>
#include <set>
#include <tuple>

using namespace BitMotor;

namespace {

Vec3 PositionOf(const Mat4& m) { return {m.M[12], m.M[13], m.M[14]}; }

void CheckVec(Vec3 a, Vec3 b, float epsilon = 1e-4f) {
    CHECK_NEAR(a.X, b.X, epsilon);
    CHECK_NEAR(a.Y, b.Y, epsilon);
    CHECK_NEAR(a.Z, b.Z, epsilon);
}

// Every triangle of a closed mesh around `centre` faces away from it.
bool FacesOut(const ModelFile::Mesh& mesh, Vec3 centre = {}) {
    for (std::uint32_t f = 0; f < std::uint32_t(Modeling::FaceCount(mesh)); ++f) {
        const Vec3 n = Modeling::FaceNormal(mesh, f);
        if (n == Vec3{}) continue;
        if (Dot(n, Modeling::FaceCentre(mesh, f) - centre) < -1e-5f) return false;
    }
    return true;
}

// Every edge of the mesh is shared by exactly two faces going opposite ways (matched by position).
bool Closed(const ModelFile::Mesh& mesh) {
    std::map<std::pair<std::tuple<long, long, long>, std::tuple<long, long, long>>, int> edges;
    auto key = [](Vec3 p) {
        return std::tuple{std::lround(p.X * 1e4), std::lround(p.Y * 1e4), std::lround(p.Z * 1e4)};
    };
    for (std::size_t t = 0; t + 2 < mesh.Indices.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const Vec3 a = mesh.Vertices[mesh.Indices[t + std::size_t(k)]].Position,
                       b = mesh.Vertices[mesh.Indices[t + std::size_t((k + 1) % 3)]].Position;
            if (key(a) != key(b)) ++edges[{key(a), key(b)}];
        }
    for (const auto& [edge, count] : edges) {
        auto back = edges.find({edge.second, edge.first});
        if (count != 1 || back == edges.end() || back->second != 1) return false;
    }
    return true;
}

constexpr const char* Example = R"({
  "Format": "bitmodel",
  "Version": 1,
  "Materials": {
    "wood":  {"Color": [0.55, 0.35, 0.2, 1]},
    "glow":  {"Color": [1, 0.9, 0.4, 1], "Emission": 1, "Unlit": true},
    "skin":  {"Texture": "hero.png", "Filter": "linear"}
  },
  "Meshes": {
    "crate": {
      "Material": "wood",
      "Positions": [-0.5, -0.5, 0.5,  0.5, -0.5, 0.5,  0.5, 0.5, 0.5,  -0.5, 0.5, 0.5],
      "Normals":   [0, 0, 1,  0, 0, 1,  0, 0, 1,  0, 0, 1],
      "Uvs":       [0, 1,  1, 1,  1, 0,  0, 0],
      "Colors":    [1, 0, 0, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1],
      "Indices":   [0, 1, 2,  0, 2, 3]
    }
  },
  "Nodes": [
    {"Name": "root", "Parent": -1},
    {"Name": "lid", "Parent": 0, "Mesh": "crate", "Position": [0, 0.5, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 0.2, 1]}
  ],
  "Animations": {
    "open": {
      "Duration": 0.5,
      "Loop": false,
      "Tracks": [
        {"Node": "lid", "Property": "Rotation", "Interpolation": "Linear",
         "Times": [0, 0.5], "Values": [0, 0, 0, 1,  -0.5, 0, 0, 0.866]},
        {"Node": "root", "Property": "Position", "Interpolation": "Step", "Times": [0, 0.25], "Values": [0, 0, 0, 0, 1, 0]}
      ],
      "Events": {"creak": [0.1]}
    }
  },
  "Sockets": {
    "handle": {"Node": "lid", "Position": [0, 0.1, -0.5], "Rotation": [0, 0, 0, 1]}
  }
})";

} // namespace

// ---------------------------------------------------------------- ModelFile

TEST(ModelFileReadsEverything) {
    const auto file = ModelFile::Parse(Example);
    CHECK(file.has_value());
    CHECK_EQ(file->Materials.size(), std::size_t(3));
    CHECK_NEAR(file->Materials.at("wood").Tint.G, 0.35f, 1e-6f);
    CHECK(file->Materials.at("glow").Unlit);
    CHECK_EQ(file->Materials.at("skin").Texture, std::string("hero.png"));
    CHECK(file->Materials.at("skin").TextureFilter == Filter::Linear);
    const ModelFile::Mesh& crate = file->Meshes.at("crate");
    CHECK_EQ(crate.Vertices.size(), std::size_t(4));
    CHECK_EQ(crate.Indices.size(), std::size_t(6));
    CHECK_NEAR(crate.Vertices[0].Tint.G, 0.0f, 1e-6f);
    CHECK_NEAR(crate.Vertices[0].Uv.Y, 1.0f, 1e-6f);
    CHECK_EQ(file->Nodes.size(), std::size_t(2));
    CHECK_EQ(file->Nodes[1].Parent, 0);
    CHECK_EQ(file->FindNode("lid"), 1);
    CHECK_EQ(file->FindNode("nobody"), -1);
    const ModelFile::Animation& open = file->Animations.at("open");
    CHECK(!open.Loop);
    CHECK_NEAR(open.Length(), 0.5f, 1e-6f);
    CHECK_EQ(open.Tracks.size(), std::size_t(2));
    CHECK(open.Tracks[1].Step);
    CHECK_EQ(open.Events.at("creak").size(), std::size_t(1));
    CHECK_EQ(file->Sockets.at("handle").Node, std::string("lid"));
}

TEST(ModelFileRefusesWhatIsNotABitmodel) {
    CHECK(!ModelFile::Parse("not json").has_value());
    CHECK(!ModelFile::Parse(R"({"Format": "bitmusic"})").has_value());
    CHECK(ModelFile::Parse(R"({"Format": "bitmodel"})").has_value());
}

TEST(ModelFileSavesTheSameBytesItReads) {
    const auto file = ModelFile::Parse(Example);
    const std::string once = file->ToJson();
    const auto again = ModelFile::Parse(once);
    CHECK(again.has_value());
    CHECK(*again == *file);
    CHECK_EQ(again->ToJson(), once);
    // Key order of the document, defaults left out, numbers short.
    CHECK(once.find("\"Format\": \"bitmodel\"") < once.find("\"Materials\""));
    CHECK(once.find("\"Materials\"") < once.find("\"Meshes\""));
    CHECK(once.find("\"Meshes\"") < once.find("\"Nodes\""));
    CHECK(once.find("\"Nodes\"") < once.find("\"Animations\""));
    CHECK(once.find("\"Animations\"") < once.find("\"Sockets\""));
    CHECK(once.find("0.55") != std::string::npos);
    CHECK(once.find("\"Rotation\": [0, 0, 0, 1]") == std::string::npos); // the default rotation is not written
    CHECK(once.find("\"Interpolation\": \"Linear\"") == std::string::npos);
}

TEST(ModelFileWritesNumbersWithSixDecimals) {
    ModelFile file;
    ModelFile::Node node;
    node.Name = "n";
    node.Position = {0.1f, -0.0f, 1.0f / 3.0f};
    file.Nodes.push_back(node);
    const std::string text = file.ToJson();
    CHECK(text.find("[0.1, 0, 0.333333]") != std::string::npos);
}

TEST(ModelFileBecomesTheSameModelAsParse) {
    const auto file = ModelFile::Parse(Example);
    const Model a = Model::Parse(Example), b = Model::FromFile(*file);
    CHECK(a && b);
    CHECK(a.Nodes() == b.Nodes());
    for (const float t : {0.0f, 0.1f, 0.3f, 0.5f}) {
        const auto pa = a.Pose("open", t), pb = b.Pose("open", t), pc = Modeling::Pose(*file, "open", t);
        CHECK_EQ(pa.size(), pc.size());
        for (std::size_t i = 0; i < pa.size(); ++i)
            for (int k = 0; k < 16; ++k) {
                CHECK_NEAR(pa[i].M[k], pb[i].M[k], 1e-6f);
                CHECK_NEAR(pa[i].M[k], pc[i].M[k], 1e-6f);
            }
    }
    CHECK(a.Socket("handle").has_value() && b.Socket("handle").has_value());
}

TEST(ModelFileSavesToDisk) {
    const auto file = ModelFile::Parse(Example);
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "BitMotorTests" / "model.bitmodel";
    CHECK(file->Save(path));
    const auto back = ModelFile::Load(path);
    CHECK(back.has_value());
    CHECK(*back == *file);
}

// ---------------------------------------------------------------- primitives

TEST(ModelingPrimitivesAreClosedAndFaceOut) {
    for (const ModelFile::Mesh& mesh :
         {Modeling::Cube(), Modeling::Sphere(0.5f, 12), Modeling::Cylinder(0.5f, 1, 8), Modeling::Cone(0.5f, 1, 8)}) {
        CHECK(FacesOut(mesh));
        ModelFile::Mesh welded = mesh;
        Modeling::ClearNormals(welded);
        CHECK(Closed(welded));
        ModelFile one;
        one.Meshes["m"] = mesh;
        CHECK(Modeling::Check(one).empty()); // no slivers at the poles
    }
    const ModelFile::Mesh plane = Modeling::Plane({2, 2}, 3);
    CHECK_EQ(Modeling::FaceCount(plane), 18);
    CHECK_EQ(Modeling::FacesFacing(plane, {0, 1, 0}).size(), std::size_t(18));
    CheckVec(Modeling::BoundsOf(plane).Max, {1, 0, 1});
}

TEST(ModelingWeldsAndNormals) {
    ModelFile::Mesh cube = Modeling::Cube();
    CHECK_EQ(cube.Vertices.size(), std::size_t(24));
    Modeling::Weld(cube);
    CHECK_EQ(cube.Vertices.size(), std::size_t(24)); // the normals differ
    for (Vertex3D& v : cube.Vertices) v.Uv = {};
    Modeling::ClearNormals(cube);
    CHECK_EQ(cube.Vertices.size(), std::size_t(8));
    CHECK_EQ(Modeling::FaceCount(cube), 12);
    Modeling::SmoothNormals(cube);
    CHECK_EQ(cube.Vertices.size(), std::size_t(8));
    CHECK_NEAR(Length(cube.Vertices[0].Normal), 1.0f, 1e-5f);
    Modeling::FlatNormals(cube);
    CHECK_EQ(cube.Vertices.size(), std::size_t(24));
    CHECK(FacesOut(cube));
}

// ---------------------------------------------------------------- edits

TEST(ModelingExtrudesTheTopOfACube) {
    ModelFile::Mesh cube = Modeling::Cube();
    const Modeling::Faces top = Modeling::FacesFacing(cube, {0, 1, 0});
    CHECK_EQ(top.size(), std::size_t(2));
    const Modeling::Faces moved = Modeling::Extrude(cube, top, 0.5f);
    CHECK(moved == top);
    CHECK_EQ(Modeling::FaceCount(cube), 12 + 8);
    CheckVec(Modeling::BoundsOf(cube).Max, {0.5f, 1, 0.5f});
    CHECK(FacesOut(cube, {0, 0.25f, 0}));
    ModelFile::Mesh welded = cube;
    Modeling::ClearNormals(welded);
    CHECK(Closed(welded));
    // Inwards: a hole.
    ModelFile::Mesh hole = Modeling::Cube();
    const Modeling::Faces lid = Modeling::FacesFacing(hole, {0, 1, 0});
    Modeling::Extrude(hole, Modeling::Inset(hole, lid, 0.3f), -0.2f);
    CheckVec(Modeling::BoundsOf(hole).Max, {0.5f, 0.5f, 0.5f});
    CHECK_NEAR(Modeling::BoundsOf(hole).Min.Y, -0.5f, 1e-5f);
}

TEST(ModelingInsetsAndSubdivides) {
    ModelFile::Mesh plane = Modeling::Plane({1, 1});
    const Modeling::Faces inner = Modeling::Inset(plane, Modeling::Faces{0}, 0.5f);
    CHECK_EQ(inner.size(), std::size_t(1));
    CHECK_EQ(Modeling::FaceCount(plane), 2 + 6);
    CHECK_EQ(Modeling::FacesFacing(plane, {0, 1, 0}).size(), std::size_t(8));
    ModelFile::Mesh tri = Modeling::Plane({1, 1});
    const Modeling::Faces four = Modeling::Subdivide(tri, Modeling::Faces{0, 1});
    CHECK_EQ(four.size(), std::size_t(8));
    CHECK_EQ(Modeling::FaceCount(tri), 8);
    CHECK_EQ(tri.Vertices.size(), std::size_t(4 + 5)); // the shared diagonal's middle only once
    CHECK_EQ(Modeling::FacesFacing(tri, {0, 1, 0}).size(), std::size_t(8));
}

TEST(ModelingPaintsOnlyTheChosenFaces) {
    ModelFile::Mesh cube = Modeling::Cube();
    Modeling::ClearNormals(cube); // shared corners
    const Modeling::Faces top = Modeling::FacesFacing(cube, {0, 1, 0});
    Modeling::Paint(cube, top, Colors::Red);
    for (std::uint32_t f = 0; f < 12; ++f) {
        const bool chosen = std::find(top.begin(), top.end(), f) != top.end();
        for (int k = 0; k < 3; ++k)
            CHECK(cube.Vertices[cube.Indices[f * 3 + std::uint32_t(k)]].Tint == (chosen ? Colors::Red : Colors::White));
    }
    Modeling::PaletteUv(cube, top, 5, 4, 2);
    CheckVec({cube.Vertices[cube.Indices[top[0] * 3]].Uv.X, cube.Vertices[cube.Indices[top[0] * 3]].Uv.Y, 0},
             {1.5f / 4, 1.5f / 2, 0});
    Modeling::BoxUv(cube, Modeling::AllFaces(cube), 1);
    CHECK_EQ(Modeling::FaceCount(cube), 12);
}

TEST(ModelingFlipsDeletesMirrorsAndAppends) {
    ModelFile::Mesh cube = Modeling::Cube();
    Modeling::Flip(cube, Modeling::AllFaces(cube));
    CHECK(!FacesOut(cube));
    Modeling::Flip(cube, Modeling::AllFaces(cube));
    CHECK(FacesOut(cube));
    Modeling::Delete(cube, Modeling::FacesFacing(cube, {0, 1, 0}));
    CHECK_EQ(Modeling::FaceCount(cube), 10);
    CHECK_EQ(cube.Vertices.size(), std::size_t(20));

    ModelFile::Mesh half = Modeling::Cube();
    Modeling::TransformAll(half, Mat4::Translate({0.5f, 0, 0})); // from x 0 to 1
    Modeling::Delete(half, Modeling::FacesFacing(half, {-1, 0, 0}));
    Modeling::Mirror(half, 0);
    CHECK_EQ(Modeling::FaceCount(half), 20);
    CheckVec(Modeling::BoundsOf(half).Min, {-1, -0.5f, -0.5f});
    CHECK(FacesOut(half));
    ModelFile::Mesh both = Modeling::Cube();
    const Modeling::Faces added = Modeling::Append(both, Modeling::Cube(), Mat4::Translate({3, 0, 0}));
    CHECK_EQ(added.size(), std::size_t(12));
    CHECK_EQ(added.front(), 12u);
    CHECK_EQ(Modeling::Connected(both, Modeling::Faces{0}).size(), std::size_t(12));
    // A mirroring transform keeps the faces facing out.
    ModelFile::Mesh flipped = Modeling::Cube();
    Modeling::TransformAll(flipped, Mat4::Scale({-1, 1, 1}));
    CHECK(FacesOut(flipped));
}

TEST(ModelingMovesCornersTogether) {
    ModelFile::Mesh cube = Modeling::Cube();
    const auto corner = Modeling::SamePlace(cube, std::vector<std::uint32_t>{0});
    CHECK_EQ(corner.size(), std::size_t(3)); // three faces meet at a cube's corner
    Modeling::Transform(cube, corner, Mat4::Translate({0, -1, 0}));
    CHECK_NEAR(Modeling::BoundsOf(cube).Min.Y, -1.5f, 1e-5f);
}

TEST(ModelingRaycastsFaces) {
    const ModelFile::Mesh cube = Modeling::Cube();
    const auto hit = Modeling::Raycast(cube, Mat4::Translate({0, 1, 0}), {0, 5, 0}, {0, -1, 0});
    CHECK(hit.has_value());
    CHECK_NEAR(hit->Distance, 3.5f, 1e-5f);
    CheckVec(hit->Point, {0, 1.5f, 0});
    CHECK(Modeling::FaceNormal(cube, hit->Face).Y > 0.99f);
    CHECK(!Modeling::Raycast(cube, {}, {5, 5, 5}, {0, 1, 0}).has_value());
}

TEST(ModelingIsDeterministic) {
    auto build = [] {
        ModelFile file;
        file.Meshes["tower"] = Modeling::Cylinder(0.5f, 2, 8);
        ModelFile::Mesh& tower = file.Meshes["tower"];
        auto top = Modeling::Inset(tower, Modeling::FacesFacing(tower, {0, 1, 0}), 0.3f);
        Modeling::Extrude(tower, top, -0.2f);
        Modeling::Subdivide(tower, top);
        Modeling::Paint(tower, top, Colors::Slate);
        ModelFile::Node node;
        node.Name = "tower";
        node.Mesh = "tower";
        file.Nodes.push_back(node);
        return file.ToJson();
    };
    CHECK_EQ(build(), build());
}

// ---------------------------------------------------------------- the model as a whole

TEST(ModelingRenamesAndKeepsReferences) {
    auto file = *ModelFile::Parse(Example);
    CHECK(Modeling::Rename(file, Modeling::Part::Node, "lid", "top"));
    CHECK_EQ(file.Nodes[1].Name, std::string("top"));
    CHECK_EQ(file.Animations.at("open").Tracks[0].Node, std::string("top"));
    CHECK_EQ(file.Sockets.at("handle").Node, std::string("top"));
    CHECK(Modeling::Rename(file, Modeling::Part::Mesh, "crate", "box"));
    CHECK_EQ(file.Nodes[1].Mesh, std::string("box"));
    CHECK(Modeling::Rename(file, Modeling::Part::Material, "wood", "oak"));
    CHECK_EQ(file.Meshes.at("box").Material, std::string("oak"));
    CHECK(!Modeling::Rename(file, Modeling::Part::Material, "oak", "glow")); // taken
    CHECK(!Modeling::Rename(file, Modeling::Part::Socket, "nothing", "x"));
    CHECK_EQ(Modeling::UniqueName(file, Modeling::Part::Node, "top"), std::string("top2"));
    CHECK_EQ(Modeling::UniqueName(file, Modeling::Part::Node, "arm"), std::string("arm"));
    CHECK(Modeling::Check(file).empty());
}

TEST(ModelingRemovesNodesKeepingTheirChildrenInPlace) {
    ModelFile file;
    ModelFile::Node a, b, c;
    a.Name = "a", a.Position = {1, 0, 0}, a.Rotation = Quat::FromAxisAngle({0, 1, 0}, Pi / 2);
    b.Name = "b", b.Parent = 0, b.Position = {0, 0, -2};
    c.Name = "c", c.Parent = 1, c.Position = {0, 1, 0};
    file.Nodes = {a, b, c};
    file.Sockets["tip"] = {"b", {}, {}};
    const Vec3 before = PositionOf(Modeling::Pose(file)[2]);
    Modeling::RemoveNode(file, 1);
    CHECK_EQ(file.Nodes.size(), std::size_t(2));
    CHECK_EQ(file.Nodes[1].Name, std::string("c"));
    CHECK_EQ(file.Nodes[1].Parent, 0);
    CheckVec(PositionOf(Modeling::Pose(file)[1]), before);
    CHECK(file.Sockets.empty());
}

TEST(ModelingReparentsKeepingParentsFirst) {
    ModelFile file;
    for (const char* n : {"root", "arm", "hand"}) {
        ModelFile::Node node;
        node.Name = n;
        file.Nodes.push_back(node);
    }
    file.Nodes[2].Parent = 0;
    // The arm goes under the hand, which comes after it: the arm moves after the hand.
    CHECK(Modeling::SetParent(file, 1, 2));
    CHECK(file.FindNode("hand") < file.FindNode("arm"));
    CHECK_EQ(file.Nodes[std::size_t(file.FindNode("arm"))].Parent, file.FindNode("hand"));
    CHECK_EQ(file.Nodes[std::size_t(file.FindNode("hand"))].Parent, file.FindNode("root"));
    // A loop is refused.
    CHECK(!Modeling::SetParent(file, file.FindNode("root"), file.FindNode("arm")));
    CHECK(!Modeling::SetParent(file, 0, 0));
    CHECK(Modeling::Check(file).empty());
}

TEST(ModelingSetsAndRemovesKeys) {
    ModelFile file;
    ModelFile::Node node;
    node.Name = "box";
    node.Position = {0, 1, 0};
    file.Nodes.push_back(node);
    using P = ModelFile::Property;
    CHECK(Modeling::SetKey(file, "jump", "box", P::Position, 1, std::vector<float>{0, 3, 0}));
    CHECK(Modeling::SetKey(file, "jump", "box", P::Position, 0, std::vector<float>{0, 1, 0}));
    CHECK(Modeling::SetKey(file, "jump", "box", P::Position, 1, std::vector<float>{0, 2, 0})); // replaces
    CHECK(!Modeling::SetKey(file, "jump", "box", P::Rotation, 0, std::vector<float>{0, 0, 0}));
    const ModelFile::Track& track = file.Animations.at("jump").Tracks.at(0);
    CHECK(track.Times == (std::vector<float>{0, 1}));
    CHECK(track.Values == (std::vector<float>{0, 1, 0, 0, 2, 0}));
    CHECK_NEAR(Modeling::ValueAt(file, "jump", "box", P::Position, 0.5f)[1], 1.5f, 1e-5f);
    CHECK_NEAR(PositionOf(Modeling::Pose(file, "jump", 0.5f)[0]).Y, 1.5f, 1e-5f);
    CHECK_NEAR(Modeling::ValueAt(file, "jump", "box", P::Scale, 0.5f)[0], 1.0f, 1e-5f); // no track: rest
    CHECK(Modeling::RemoveKey(file, "jump", "box", P::Position, 0));
    CHECK(Modeling::RemoveKey(file, "jump", "box", P::Position, 1));
    CHECK(file.Animations.at("jump").Tracks.empty());
    CHECK(!Modeling::RemoveKey(file, "jump", "box", P::Position, 1));
}

TEST(ModelingChecksModels) {
    auto file = *ModelFile::Parse(Example);
    CHECK(Modeling::Check(file).empty());
    file.Nodes[1].Mesh = "nothing";
    file.Meshes["crate"].Material = "gone";
    file.Meshes["crate"].Indices.push_back(99);
    file.Animations["open"].Tracks[0].Values.pop_back();
    file.Sockets["handle"].Node = "ghost";
    std::set<Modeling::Issue> found;
    for (const Modeling::Problem& p : Modeling::Check(file)) found.insert(p.Issue);
    for (const Modeling::Issue issue :
         {Modeling::Issue::MissingMesh, Modeling::Issue::MissingMaterial, Modeling::Issue::BadIndex,
          Modeling::Issue::BadTrack, Modeling::Issue::MissingNode, Modeling::Issue::UnusedMesh})
        CHECK(found.contains(issue));
}

// ---------------------------------------------------------------- OBJ

TEST(ModelingImportsObj) {
    const char* obj = R"(# a quad and a triangle
mtllib scene.mtl
v -1 0 1
v 1 0 1
v 1 0 -1
v -1 0 -1 1 0 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 1 0
o floor
usemtl stone
f 1/1/1 2/2/1 3/3/1 4/4/1
o roof
usemtl stone
f -4//1 -3//1 -2//1
)";
    const char* mtl = R"(newmtl stone
Kd 0.5 0.5 0.6
d 0.75
Ke 0.2 0.1 0
map_Kd textures\stone.png
)";
    const auto file = Modeling::ImportObj(obj, mtl, "scene.obj");
    CHECK(file.has_value());
    CHECK_EQ(file->Meshes.size(), std::size_t(2));
    CHECK_EQ(file->Nodes.size(), std::size_t(2));
    const ModelFile::Mesh& floor = file->Meshes.at("floor");
    CHECK_EQ(Modeling::FaceCount(floor), 2);
    CHECK_EQ(floor.Material, std::string("stone"));
    CHECK_NEAR(floor.Vertices[0].Uv.Y, 1.0f, 1e-6f); // V flipped
    CHECK(floor.Vertices[3].Tint == (Color{1, 0, 0, 1}));
    CHECK_EQ(Modeling::FacesFacing(floor, {0, 1, 0}).size(), std::size_t(2));
    const ModelFile::Material& stone = file->Materials.at("stone");
    CHECK_NEAR(stone.Tint.B, 0.6f, 1e-6f);
    CHECK_NEAR(stone.Tint.A, 0.75f, 1e-6f);
    CHECK_NEAR(stone.Emission, 0.2f, 1e-6f);
    CHECK_EQ(stone.Texture, std::string("textures/stone.png"));
    CHECK(!Modeling::ImportObj("v 0 0 0\n").has_value());
}

TEST(ModelingExportsObjThatReadsBack) {
    auto file = *ModelFile::Parse(Example);
    const Modeling::ObjFiles out = Modeling::ExportObj(file, "crate.mtl");
    CHECK(out.Obj.find("mtllib crate.mtl") != std::string::npos);
    CHECK(out.Obj.find("o lid") != std::string::npos);
    CHECK(out.Mtl.find("newmtl wood") != std::string::npos);
    const auto back = Modeling::ImportObj(out.Obj, out.Mtl);
    CHECK(back.has_value());
    const ModelFile::Mesh& lid = back->Meshes.at("lid");
    CHECK_EQ(Modeling::FaceCount(lid), 2);
    // Baked in the pose: the lid's node is at y 0.5, scaled 0.2 in Y.
    CHECK_NEAR(Modeling::BoundsOf(lid).Max.Y, 0.5f + 0.1f, 1e-5f);
    CHECK_NEAR(back->Materials.at("wood").Tint.R, 0.55f, 1e-6f);
    CHECK_NEAR(back->Materials.at("glow").Emission, 1.0f, 1e-6f);
    CHECK(back->Materials.at("glow").Unlit);
    CHECK_EQ(Modeling::ExportObj(file).Obj, Modeling::ExportObj(file).Obj);
}

// ---------------------------------------------------------------- glTF

namespace {

std::string Base64Of(const std::vector<std::uint8_t>& bytes) {
    static const char* abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const unsigned n = unsigned(bytes[i]) << 16 | (i + 1 < bytes.size() ? unsigned(bytes[i + 1]) << 8 : 0u) |
                           (i + 2 < bytes.size() ? unsigned(bytes[i + 2]) : 0u);
        out += abc[n >> 18 & 63];
        out += abc[n >> 12 & 63];
        out += i + 1 < bytes.size() ? abc[n >> 6 & 63] : '=';
        out += i + 2 < bytes.size() ? abc[n & 63] : '=';
    }
    return out;
}

template <class T> void Put(std::vector<std::uint8_t>& out, T value) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), p, p + sizeof(T));
}

std::vector<std::uint8_t> TextBytes(const std::string& text) { return {text.begin(), text.end()}; }

} // namespace

TEST(ModelingGlbGoesThereAndBack) {
    auto file = *ModelFile::Parse(Example);
    file.Materials["glow"].Emission = 2;
    const std::vector<std::uint8_t> glb = Modeling::ExportGlb(file);
    CHECK(glb.size() > 20);
    CHECK(glb == Modeling::ExportGlb(file)); // the same bytes every time
    const auto back = Modeling::ImportGltf(glb, {}, "crate");
    CHECK(back.has_value());
    CHECK(back->Notes.empty());
    const ModelFile& b = back->File;
    CHECK_EQ(b.Nodes.size(), file.Nodes.size());
    CHECK_EQ(b.Nodes[1].Name, std::string("lid"));
    CHECK_EQ(b.Nodes[1].Parent, 0);
    CheckVec(b.Nodes[1].Scale, {1, 0.2f, 1});
    const ModelFile::Mesh& crate = b.Meshes.at("crate");
    CHECK_EQ(Modeling::FaceCount(crate), 2);
    CHECK_EQ(crate.Material, std::string("wood"));
    CHECK_NEAR(crate.Vertices[0].Tint.R, 1.0f, 1e-4f);
    CHECK_NEAR(b.Materials.at("wood").Tint.G, 0.35f, 1e-4f); // through linear and back
    CHECK_NEAR(b.Materials.at("glow").Emission, 2.0f, 1e-5f);
    CHECK(b.Materials.at("glow").Unlit);
    CHECK_EQ(b.Materials.at("skin").Texture, std::string("hero.png"));
    CHECK(b.Materials.at("skin").TextureFilter == Filter::Linear);
    const ModelFile::Animation& open = b.Animations.at("open");
    CHECK(!open.Loop);
    CHECK_NEAR(open.Length(), 0.5f, 1e-6f);
    CHECK_EQ(open.Tracks.size(), std::size_t(2));
    CHECK(open.Tracks[1].Step);
    CHECK_EQ(open.Events.at("creak").size(), std::size_t(1));
    CHECK_EQ(b.Sockets.at("handle").Node, std::string("lid"));
    for (const float t : {0.0f, 0.2f, 0.5f}) {
        const auto pa = Modeling::Pose(file, "open", t), pb = Modeling::Pose(b, "open", t);
        for (std::size_t i = 0; i < pa.size(); ++i)
            for (int k = 0; k < 16; ++k) CHECK_NEAR(pa[i].M[k], pb[i].M[k], 1e-4f);
    }
}

TEST(ModelingReadsGltfText) {
    // Two primitives (a triangle and a line), positions as floats, indices as bytes, a matrix node and a cubic
    // rotation curve, all in a data: buffer.
    std::vector<std::uint8_t> bin;
    for (const float f : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f}) Put(bin, f); // 0: 36 bytes
    for (const int i : {0, 1, 2, 0}) Put(bin, std::uint8_t(i));                      // 36: 3 indices + pad
    for (const float f : {0.f, 1.f}) Put(bin, f);                                    // 40: times
    for (int k = 0; k < 2; ++k)                                                      // 48: 2 x (in, v, out)
        for (int part = 0; part < 3; ++part)
            for (const float f : {0.f, 0.f, 0.f, 1.f}) Put(bin, f);
    const std::string gltf = R"({
      "asset": {"version": "2.0"},
      "buffers": [{"byteLength": )" +
                             std::to_string(bin.size()) + R"(, "uri": "data:application/octet-stream;base64,)" +
                             Base64Of(bin) + R"("}],
      "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}, {"buffer": 0, "byteOffset": 36, "byteLength": 3},
                      {"buffer": 0, "byteOffset": 40, "byteLength": 8}, {"buffer": 0, "byteOffset": 48, "byteLength": 96}],
      "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
                    {"bufferView": 1, "componentType": 5121, "count": 3, "type": "SCALAR"},
                    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "SCALAR"},
                    {"bufferView": 3, "componentType": 5126, "count": 6, "type": "VEC4"}],
      "materials": [{"name": "red", "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1]}}],
      "meshes": [{"name": "tri", "primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "material": 0},
                                              {"attributes": {"POSITION": 0}, "mode": 1}]}],
      "nodes": [{"name": "holder", "children": [1]},
                {"name": "shape", "mesh": 0, "matrix": [2,0,0,0, 0,2,0,0, 0,0,2,0, 1,2,3,1], "skin": 0}],
      "scenes": [{"nodes": [0]}],
      "animations": [{"name": "spin", "samplers": [{"input": 2, "output": 3, "interpolation": "CUBICSPLINE"}],
                      "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}]}]
    })";
    const auto result = Modeling::ImportGltf(TextBytes(gltf), {}, "test");
    CHECK(result.has_value());
    const ModelFile& f = result->File;
    CHECK_EQ(f.Nodes.size(), std::size_t(2));
    CHECK_EQ(f.Nodes[1].Name, std::string("shape"));
    CHECK_EQ(f.Nodes[1].Parent, 0);
    CheckVec(f.Nodes[1].Position, {1, 2, 3});
    CheckVec(f.Nodes[1].Scale, {2, 2, 2});
    CHECK_EQ(f.Meshes.size(), std::size_t(1)); // the line was left out
    CHECK_EQ(f.Meshes.at("tri.0").Material, std::string("red"));
    CHECK_EQ(Modeling::FaceCount(f.Meshes.at("tri.0")), 1);
    CHECK_EQ(f.Animations.at("spin").Tracks.at(0).Values.size(), std::size_t(8));
    CHECK(result->Notes.size() >= 3); // the line, the skin, the cubic curve
    CHECK(Modeling::Check(f).empty());
    CHECK(!Modeling::ImportGltf(TextBytes("{}")).has_value());
}
