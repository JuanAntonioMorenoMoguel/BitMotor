#include "Printing.h"
#include "Testing.h"

#include "Render/ModelInternal.h"

#include <BitMotor/BitMotor.h>

#include <cmath>

using namespace BitMotor;

namespace {

void CheckVec(Vec3 a, Vec3 b, float epsilon = 1e-4f) {
    CHECK_NEAR(a.X, b.X, epsilon);
    CHECK_NEAR(a.Y, b.Y, epsilon);
    CHECK_NEAR(a.Z, b.Z, epsilon);
}

Vec3 PositionOf(const Mat4& m) { return {m.M[12], m.M[13], m.M[14]}; }

// Every triangle of a closed mesh centred on the origin faces away from it (counter-clockwise outside).
bool FacesOut(const Mesh& mesh) {
    const auto& v = mesh.Vertices();
    const auto& i = mesh.Indices();
    for (std::size_t t = 0; t + 2 < i.size(); t += 3) {
        const Vec3 a = v[i[t]].Position, b = v[i[t + 1]].Position, c = v[i[t + 2]].Position;
        const Vec3 n = Cross(b - a, c - a);
        if (LengthSquared(n) < 1e-12f) continue; // a sliver at a pole
        if (Dot(n, (a + b + c) / 3.0f) < 0) return false;
    }
    return true;
}

constexpr const char* Crate = R"({
  "Format": "bitmodel", "Version": 1,
  "Materials": {"wood": {"Color": [0.5, 0.25, 0.1, 1], "Emission": 0.5}},
  "Meshes": {
    "tri": {"Material": "wood", "Positions": [0, 0, 0, 1, 0, 0, 0, 1, 0]},
    "quad": {"Positions": [0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0], "Colors": [1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1],
             "Indices": [0, 1, 2, 0, 2, 3]}
  },
  "Nodes": [
    {"Name": "root", "Position": [0, 1, 0]},
    {"Name": "lid", "Parent": 0, "Mesh": "tri", "Position": [2, 0, 0]},
    {"Name": "plate", "Mesh": "quad", "Scale": [2, 2, 2]}
  ],
  "Animations": {
    "open": {"Loop": false, "Tracks": [
      {"Node": "lid", "Property": "Position", "Times": [0, 1], "Values": [2, 0, 0, 4, 0, 0]},
      {"Node": "root", "Property": "Rotation", "Interpolation": "Step", "Times": [0, 0.5],
       "Values": [0, 0, 0, 1, 0, 0.7071068, 0, 0.7071068]}
    ], "Events": {"creak": [0.25]}},
    "spin": {"Duration": 2, "Tracks": [{"Node": "lid", "Property": "Scale", "Times": [0, 1], "Values": [1, 1, 1, 3, 3, 3]}],
             "Events": {"tick": [0.5]}}
  },
  "Sockets": {"handle": {"Node": "lid", "Position": [0, 1, 0]}}
})";

} // namespace

TEST(MeshPrimitivesAreClosedAndFaceOut) {
    Mesh cube = Mesh::Cube({2, 2, 2});
    CHECK_EQ(cube.Vertices().size(), std::size_t(24));
    CHECK_EQ(cube.Indices().size(), std::size_t(36));
    CHECK(FacesOut(cube));
    CheckVec(cube.Min(), {-1, -1, -1});
    CheckVec(cube.Max(), {1, 1, 1});
    for (const Vertex3D& v : cube.Vertices()) CHECK_NEAR(Length(v.Normal), 1.0f, 1e-5f);
    CHECK(FacesOut(Mesh::Sphere(1, 12)));
    CHECK(FacesOut(Mesh::Cylinder(1, 2, 10)));
    Mesh plane = Mesh::Plane({4, 2});
    CHECK_EQ(plane.Indices().size(), std::size_t(6));
    const auto& p = plane.Vertices();
    CHECK(Cross(p[1].Position - p[0].Position, p[2].Position - p[0].Position).Y > 0); // faces up
}

TEST(MeshWithoutNormalsGetsFlatOnes) {
    Mesh mesh = Mesh::Create(
        {{.Position = {0, 0, 0}}, {.Position = {1, 0, 0}}, {.Position = {0, 0, -1}}, {.Position = {1, 0, -1}}},
        {0, 1, 2, 2, 1, 3});
    CHECK_EQ(mesh.Vertices().size(), std::size_t(6)); // one set of vertices per face
    for (const Vertex3D& v : mesh.Vertices()) CheckVec(v.Normal, {0, 1, 0});
    Mesh loose = Mesh::Create(
        {{.Position = {0, 0, 0}}, {.Position = {1, 0, 0}}, {.Position = {0, 1, 0}}}); // no indices: three by three
    CHECK_EQ(loose.Indices().size(), std::size_t(3));
    CheckVec(loose.Vertices()[0].Normal, {0, 0, 1});
    Mesh bad = Mesh::Create({{.Position = {0, 0, 0}}, {.Position = {1, 0, 0}}, {.Position = {0, 1, 0}}},
                            {0, 1, 7}); // an index out of range
    CHECK(bad.Indices().empty());
}

TEST(ModelParsesABitmodel) {
    Model model = Model::Parse(Crate, {}, "crate.bitmodel");
    CHECK(bool(model));
    if (!model) return;
    const auto& data = *model.Internal();
    CHECK_EQ(data.Meshes.size(), std::size_t(2));
    CHECK_EQ(data.Nodes.size(), std::size_t(3));
    const auto& tri = data.Meshes[std::size_t(data.Nodes[1].Mesh)];
    const auto& quad = data.Meshes[std::size_t(data.Nodes[2].Mesh)];
    CHECK_NEAR(tri.Look.Tint.R, 0.5f, 1e-6f);
    CHECK_NEAR(tri.Look.Emission, 0.5f, 1e-6f);
    CHECK_NEAR(quad.Geometry.Vertices()[0].Tint.G, 0.0f, 1e-6f);
    CHECK_NEAR(quad.Look.Tint.R, 1.0f, 1e-6f); // no material: white
    CHECK_EQ(model.Animations().size(), std::size_t(2));
    const std::vector<std::string> nodes = model.Nodes(); // a copy: CHECK_EQ keeps a reference to its argument
    CHECK_EQ(nodes[1], std::string("lid"));

    const std::vector<Mat4> rest = model.Pose();
    CheckVec(PositionOf(rest[1]), {2, 1, 0}); // child of root, moved up by it
    CheckVec(rest[2].TransformPoint({1, 0, 0}), {2, 0, 0});
}

TEST(ModelRejectsOtherFormats) {
    CHECK(!Model::Parse(R"({"Format": "gltf"})"));
    CHECK(!Model::Parse("not json"));
    CHECK(bool(Model::Parse(R"({"Format": "bitmodel"})"))); // empty but valid
}

TEST(ModelAnimationsInterpolate) {
    Model model = Model::Parse(Crate);
    // At 0.5 the root has turned a quarter around Y (step), so the lid's +X offset now points to -Z.
    const Vec3 lid = PositionOf(model.Pose("open", 0.5f)[1]);
    CheckVec(lid, {0, 1, -3});
    CheckVec(PositionOf(model.Pose("open", 0.25f)[1]), {2.5f, 1, 0}); // before the step: not turned
    CheckVec(PositionOf(model.Pose("open", 5)[1]), {0, 1, -4});       // not looping: stays at the end
    // spin loops every 2 s: at 2.5 s it is at 0.5 s.
    CHECK_NEAR(model.Pose("spin", 2.5f)[1].M[0], model.Pose("spin", 0.5f)[1].M[0], 1e-5f);
    CHECK_NEAR(model.Pose("spin", 0.5f)[1].M[0], 2.0f, 1e-5f);
    CHECK_NEAR(model.Animation("open").Duration(), 1.0f, 1e-6f); // the last time of its tracks
    CHECK_NEAR(model.Animation("spin").Duration(), 2.0f, 1e-6f);
    CHECK(!model.Animation("missing"));
}

TEST(ModelEventsFireOncePerPass) {
    Model model = Model::Parse(Crate);
    ModelAnimation open = model.Animation("open"), spin = model.Animation("spin");
    CHECK(open.Fired("creak", 0.2f, 0.3f));
    CHECK(!open.Fired("creak", 0.3f, 0.4f));
    CHECK(!open.Fired("creak", 1.2f, 1.3f)); // does not loop
    CHECK(spin.Fired("tick", 2.4f, 2.6f));   // the second pass
    CHECK(!spin.Fired("tick", 2.6f, 4.4f));
    CHECK(spin.Fired("tick", 0.5f, 0.6f));  // `from` included
    CHECK(!spin.Fired("tick", 0.4f, 0.5f)); // `to` excluded
}

TEST(ModelSocketsFollowTheirNode) {
    Model model = Model::Parse(Crate);
    auto handle = model.Socket("handle");
    CHECK(handle.has_value());
    if (handle) CheckVec(PositionOf(*handle), {2, 2, 0});
    auto moved = model.Socket("handle", "open", 1);
    if (moved) CheckVec(PositionOf(*moved), {0, 2, -4});
    CHECK(!model.Socket("nope"));
}

TEST(ScreenDrawQueuesEveryMesh) {
    Detail::ResetModels();
    Model model = Model::Parse(Crate);
    Screen::Draw(Model3D{.Source = model, .Position = {10, 0, 0}});
    Screen::Draw(Model3D{.Source = Model::FromMesh(Mesh::Cube())});
    Screen::Draw(PointLight3D{});
    CHECK_EQ(Detail::ModelQueue().size(), std::size_t(3));
    CheckVec(PositionOf(Detail::ModelQueue()[0].World), {12, 1, 0});
    CHECK_EQ(Detail::PointLightQueue().size(), std::size_t(1));
    Detail::ResetModels();
    CHECK(Detail::ModelQueue().empty());
}

TEST(Camera3DProjectsOntoTheScreen) {
    Camera3D camera{.Position = {0, 0, 5}, .Target = {0, 0, 0}};
    const Vec3 ray = camera.Ray(Screen::Size() * 0.5f);
    if (Screen::Size().X > 0) CheckVec(ray, {0, 0, -1});
    CHECK(!camera.ToScreen({0, 0, 10}).has_value()); // behind the camera
}

TEST(CamerasDrawInTheirViewport) {
    Camera3D camera{.Position = {0, 0, 10}, .Target = {0, 0, 0}, .Viewport = {100, 50, 200, 100}};
    CHECK(camera.Area().X == 100.0f && camera.Area().Width == 200.0f);
    const auto centre = camera.ToScreen({0, 0, 0});
    CHECK(centre.has_value());
    CHECK_NEAR(centre->X, 200.0f, 1e-3f);
    CHECK_NEAR(centre->Y, 100.0f, 1e-3f);
    CheckVec(camera.Ray({200, 100}), {0, 0, -1});
    CheckVec(camera.RayOrigin({200, 100}), {0, 0, 10});
    // A point to the right shows to the right, inside the viewport, and its ray goes back through it.
    const Vec2 right = *camera.ToScreen({2, 0, 0});
    CHECK(right.X > 200 && right.X < 300);
    const Vec3 ray = camera.Ray(right);
    CHECK_NEAR(Dot(ray, Normalize(Vec3{2, 0, -10})), 1.0f, 1e-4f);
    // Orthographic: parallel rays from the near plane.
    camera.Orthographic = true;
    const Vec2 side = *camera.ToScreen({2, 0, 0});
    CHECK_NEAR(camera.ToScreen({2, 0, -50})->X, side.X, 1e-3f); // no perspective
    CheckVec(camera.Ray(side), {0, 0, -1});
    CheckVec(camera.RayOrigin(side), {2, 0, 10 - camera.Near}, 1e-3f);
}

TEST(LinesAndPointsBecomeQuadsFacingTheCamera) {
    Detail::ResetModels();
    const Camera3D camera{.Position = {0, 5, 10}, .Target = {0, 0, 0}, .Viewport = {0, 0, 400, 300}};
    CHECK(Detail::OverlayMesh(camera) == nullptr);
    Screen::Draw(Line3D{.From = {-1, 0, 0}, .To = {1, 0, 0}, .Width = 2});
    Screen::Draw(Point3D{.Position = {0, 1, 0}, .OnTop = true});
    Screen::Draw(Line3D{.From = {0, 0, 20}, .To = {0, 1, 20}}); // behind the camera: left out
    const auto mesh = Detail::OverlayMesh(camera);
    CHECK(mesh != nullptr);
    CHECK_EQ(mesh->Vertices.size(), std::size_t(8));
    CHECK_EQ(mesh->Indices.size(), std::size_t(12));
    for (std::size_t t = 0; t < mesh->Indices.size(); t += 3) {
        const Vec3 a = mesh->Vertices[mesh->Indices[t]].Position, b = mesh->Vertices[mesh->Indices[t + 1]].Position,
                   c = mesh->Vertices[mesh->Indices[t + 2]].Position;
        CHECK(Dot(Cross(b - a, c - a), camera.Position - a) > 0);
    }
    // The point on top sits just in front of the camera, where it covers the same spot.
    const Vec3 p = (mesh->Vertices[4].Position + mesh->Vertices[6].Position) * 0.5f;
    CHECK(Distance(p, camera.Position) < 1);
    const Vec2 shown = *camera.ToScreen(p), wanted = *camera.ToScreen({0, 1, 0});
    CHECK_NEAR(shown.X, wanted.X, 0.01f);
    CHECK_NEAR(shown.Y, wanted.Y, 0.01f);
    Detail::ResetModels();
    CHECK(Detail::OverlayMesh(camera) == nullptr);
}
