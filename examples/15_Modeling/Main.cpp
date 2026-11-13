// 15_Modeling: making a model in code with ModelFile and Modeling::, the way BitModel (the editor) does, and
// looking at it in a 3D view inside a panel (Camera3D::Viewport) with Line3D and Point3D on top.
// - A tower is built from a cylinder: the top is inset and pushed in (a hollow), painted, a cone roof floats
//   above it and a flag waves on a pole (a node with a key-framed "wave" animation).
// - The panel on the left edits it: extrude the selected faces, subdivide them, paint them, flat or smooth
//   normals, wireframe, orthographic view, and save it as tower.bitmodel (in the save folder) or as OBJ and
//   glTF. The panel on the right shows the nodes in a TreeView (drag one onto another to hang it there) and
//   a ColorPicker for the chosen node's material, or for the paint.
// - In the view: left click picks a face (and the ones that face the same way with Shift), right drag
//   orbits, the wheel zooms. Selected faces show their corners as dots.
// Esc quits.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <format>
#include <string>
#include <vector>

using namespace BitMotor;
namespace Fx = UI::JFxStyle;

struct ModelingExample : Game {
    ModelFile file;
    Model model;
    Modeling::Faces selected;
    float yaw = 0.7f, pitch = 0.45f, distance = 7, time = 0;
    bool wireframe = true, orthographic = false;
    std::string status;
    int node = 0, treeScroll = 0;
    std::vector<bool> treeOpen;
    Color paint = Colors::Gold;

    static ModelFile MakeTower() {
        ModelFile f;
        f.Materials["stone"].Tint = Color::Hex(0xC8BCA8);
        f.Materials["roof"].Tint = Color::Hex(0xB04A3A);
        f.Materials["cloth"].Tint = Color::Hex(0x3A7BD5);
        f.Materials["cloth"].Emission = 0.2f;
        ModelFile::Mesh tower = Modeling::Cylinder(0.8f, 2.4f, 10);
        tower.Material = "stone";
        const Modeling::Faces top = Modeling::Inset(tower, Modeling::FacesFacing(tower, {0, 1, 0}), 0.25f);
        Modeling::Extrude(tower, top, -0.3f);
        Modeling::Paint(tower, top, Color::Hex(0x6E655A));
        ModelFile::Mesh roof = Modeling::Cone(1.0f, 1.0f, 10);
        roof.Material = "roof";
        ModelFile::Mesh pole = Modeling::Cylinder(0.03f, 0.8f, 6);
        ModelFile::Mesh flag = Modeling::Cube({0.5f, 0.3f, 0.02f});
        flag.Material = "cloth";
        Modeling::TransformAll(flag, Mat4::Translate({0.25f, 0.25f, 0}));
        f.Meshes = {{"tower", tower}, {"roof", roof}, {"pole", pole}, {"flag", flag}};
        auto node = [&](const char* name, int parent, Vec3 position) {
            ModelFile::Node n;
            n.Name = n.Mesh = name;
            n.Parent = parent;
            n.Position = position;
            f.Nodes.push_back(n);
        };
        node("tower", -1, {0, 1.2f, 0});
        node("roof", 0, {0, 1.9f, 0});
        node("pole", 1, {0, 0.85f, 0});
        node("flag", 2, {0, 0.1f, 0});
        for (int k = 0; k <= 4; ++k) {
            const Quat q = Quat::FromAxisAngle({0, 1, 0}, std::sin(float(k) * Pi / 2) * 0.6f);
            Modeling::SetKey(f, "wave", "flag", ModelFile::Property::Rotation, float(k) * 0.4f,
                             std::vector<float>{q.X, q.Y, q.Z, q.W});
        }
        return f;
    }

    void Rebuild() { model = Model::FromFile(file); }
    ModelFile::Mesh& Tower() { return file.Meshes["tower"]; }

    void Start() override {
        file = MakeTower();
        Rebuild();
        Fx::SetPalette(Fx::Modena(Color::Hex(0x3C3F41)));
        Lighting3D::SetSun({-0.5f, -1, -0.3f}, Colors::White, 0.8f);
        Lighting3D::SetAmbient(Color::Hex(0x404858));
    }

    Rect View() const { return {200, 0, Screen::Width() - 420.0f, float(Screen::Height())}; }

    Camera3D Camera() const {
        const Vec3 target{0, 1.5f, 0};
        const Vec3 offset{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
        return {.Position = target + offset * distance,
                .Target = target,
                .Orthographic = orthographic,
                .Height = distance * 0.8f,
                .Viewport = View()};
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        time += Time::Delta();
        const Vec2 mouse = Input::Mouse();
        if (!View().Contains(mouse)) return;
        distance = std::clamp(distance - Input::MouseWheel() * 0.5f, 3.0f, 20.0f);
        if (Input::MouseDown(MouseButton::Right)) {
            const Vec2 move = Input::MouseDelta();
            yaw -= move.X * 0.01f;
            pitch = std::clamp(pitch + move.Y * 0.01f, -1.4f, 1.4f);
        }
        if (Input::MousePressed(MouseButton::Left)) {
            const Camera3D camera = Camera();
            const Mat4 world = Modeling::Pose(file)[0];
            const auto hit = Modeling::Raycast(Tower(), world, camera.RayOrigin(mouse), camera.Ray(mouse));
            if (!hit)
                selected.clear();
            else if (Input::KeyDown(Key::LeftShift))
                selected = Modeling::FacesFacing(Tower(), Modeling::FaceNormal(Tower(), hit->Face), 5);
            else
                selected = Modeling::Faces{hit->Face};
        }
    }

    void Edited(std::string what) {
        Rebuild();
        status = std::move(what);
    }

    void DrawPanel() {
        const Rect inside = Fx::Panel({8, 8, 184, Screen::Height() - 16.0f}, "Tower");
        float y = inside.Y;
        auto row = [&](float h = 26) {
            const Rect r{inside.X, y, inside.Width, h};
            y += h + 6;
            return r;
        };
        Fx::Label(row(18), std::format("{} faces, {} selected", Modeling::FaceCount(Tower()), selected.size()));
        const bool any = !selected.empty();
        if (Fx::Button(row(), "Extrude 0.2", any)) {
            Modeling::Extrude(Tower(), selected, 0.2f);
            Edited("Extruded");
        }
        if (Fx::Button(row(), "Subdivide", any)) {
            selected = Modeling::Subdivide(Tower(), selected);
            Edited("Subdivided");
        }
        if (Fx::Button(row(), "Paint", any)) {
            Modeling::Paint(Tower(), selected, paint);
            Edited("Painted");
        }
        if (Fx::Button(row(), "Flat normals")) {
            Modeling::FlatNormals(Tower());
            selected.clear();
            Edited("Flat");
        }
        if (Fx::Button(row(), "Smooth normals")) {
            Modeling::SmoothNormals(Tower());
            selected.clear();
            Edited("Smooth");
        }
        Fx::CheckBox(row(), "Wireframe", wireframe);
        Fx::CheckBox(row(), "Orthographic", orthographic);
        if (Fx::Button(row(), "Save .bitmodel"))
            Edited(file.Save("tower.bitmodel") ? "Saved tower.bitmodel" : "Not saved");
        if (Fx::Button(row(), "Save OBJ + glTF")) {
            const Modeling::ObjFiles obj = Modeling::ExportObj(file, "tower.mtl");
            const bool ok = Files::Write(Files::SavePath("tower.obj"), obj.Obj) &&
                            Files::Write(Files::SavePath("tower.mtl"), obj.Mtl) &&
                            Files::WriteBytes(Files::SavePath("tower.glb"), Modeling::ExportGlb(file));
            Edited(ok ? "Saved tower.obj and tower.glb" : "Not saved");
        }
        if (Fx::Button(row(), "Reset")) {
            file = MakeTower();
            selected.clear();
            Edited("Reset");
        }
        Fx::Label(row(18), status);
    }

    void DrawNodes() {
        const Rect inside = Fx::Panel({Screen::Width() - 212.0f, 8, 204, Screen::Height() - 16.0f}, "Nodes");
        std::vector<Fx::TreeItem> items;
        for (const ModelFile::Node& n : file.Nodes) {
            int depth = 0;
            for (int p = n.Parent; p >= 0; p = file.Nodes[std::size_t(p)].Parent) ++depth;
            items.push_back({n.Name, depth});
        }
        Fx::TreeMove move;
        Fx::TreeView({inside.X, inside.Y, inside.Width, 120}, node, items, treeOpen, treeScroll, move);
        if (move.From >= 0 && Modeling::SetParent(file, move.From, move.To)) {
            node = file.FindNode(items[std::size_t(move.From)].Label);
            Edited("Moved " + items[std::size_t(move.From)].Label);
        }
        // The chosen node's material, or the paint when it has none.
        const ModelFile::Node& chosen = file.Nodes[std::size_t(std::clamp(node, 0, int(file.Nodes.size()) - 1))];
        const std::string material = file.Meshes[chosen.Mesh].Material;
        Fx::Label({inside.X, inside.Y + 128, inside.Width, 18}, material.empty() ? "Paint" : "Material " + material);
        Color& color = material.empty() ? paint : file.Materials[material].Tint;
        const std::vector<Color> swatches = {Colors::Gold,         Color::Hex(0xC8BCA8), Color::Hex(0xB04A3A),
                                             Color::Hex(0x3A7BD5), Colors::White,        Colors::Black};
        if (Fx::ColorPicker({inside.X, inside.Y + 150, inside.Width, inside.Bottom() - inside.Y - 150}, color,
                            swatches))
            Rebuild();
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x2B2B2B));
        const Camera3D camera = Camera();
        Screen::SetCamera3D(camera);
        Screen::Draw(Model3D{.Source = model, .Animation = "wave", .Time = time});

        // The floor grid and the axes.
        for (int i = -5; i <= 5; ++i) {
            const Color c = i == 0 ? Color::Hex(0x707070) : Color::Hex(0x4A4A4A);
            Screen::Draw(Line3D{.From = {float(i), 0, -5}, .To = {float(i), 0, 5}, .Color = c});
            Screen::Draw(Line3D{.From = {-5, 0, float(i)}, .To = {5, 0, float(i)}, .Color = c});
        }
        Screen::Draw(Line3D{.From = {0, 0, 0}, .To = {1, 0, 0}, .Color = Colors::Red, .Width = 2, .OnTop = true});
        Screen::Draw(Line3D{.From = {0, 0, 0}, .To = {0, 1, 0}, .Color = Colors::Lime, .Width = 2, .OnTop = true});
        Screen::Draw(Line3D{.From = {0, 0, 0}, .To = {0, 0, 1}, .Color = Colors::Blue, .Width = 2, .OnTop = true});

        // The tower's edges and the selection.
        const Mat4 world = Modeling::Pose(file)[0];
        const ModelFile::Mesh& tower = Tower();
        if (wireframe)
            for (std::size_t t = 0; t + 2 < tower.Indices.size(); t += 3)
                for (int k = 0; k < 3; ++k) {
                    const Vec3 a = world.TransformPoint(tower.Vertices[tower.Indices[t + std::size_t(k)]].Position);
                    const Vec3 b =
                        world.TransformPoint(tower.Vertices[tower.Indices[t + std::size_t((k + 1) % 3)]].Position);
                    Screen::Draw(Line3D{.From = a, .To = b, .Color = Color{0, 0, 0, 0.5f}});
                }
        for (const std::uint32_t v : Modeling::Corners(tower, selected))
            Screen::Draw(Point3D{
                .Position = world.TransformPoint(tower.Vertices[v].Position), .Color = Colors::Orange, .OnTop = true});

        DrawPanel();
        DrawNodes();
        Text::Draw("Left click: pick  Shift: same side  Right drag: orbit  Wheel: zoom", {208, 8},
                   {.Shadow = Colors::Black});
    }
};

int main() {
    return Run<ModelingExample>(
        {.Title = "BitMotor - 15 Modeling", .Width = 960, .Height = 540, .IntegerScaling = false, .TickRate = 0});
}
