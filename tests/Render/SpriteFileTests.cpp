#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Render/SpriteFile.h"

#include <nlohmann/json.hpp>

using namespace BitMotor;

namespace {

// A 4x4 sprite: "red" pixels in frame 0, frame 1 linked to it, frame 2 a box of "steel" in a pieces layer.
SpriteFile Small() {
    SpriteFile s;
    s.Size = {4, 4};
    s.Pivot = {2, 4};
    s.Palette = {{"clear", Color(0, 0, 0, 0)}, {"red", Color(1, 0, 0, 1)}, {"steel", Color(0.4f, 0.4f, 0.4f, 1)},
                 {"lamp", Color(0, 1, 0, 1), 1}};
    s.Layers = {{"art", "pixels"}};
    s.Frames = {{100}, {100}, {200}};
    SpriteCel a;
    const std::uint8_t two[] = {1, 1};
    a.SetIndices({1, 1}, {2, 1}, two);
    s.Cels.push_back(a);
    SpriteCel link;
    link.Frame = 1;
    link.Link = 0;
    s.Cels.push_back(link);
    return s;
}

struct Rgba {
    int R, G, B, A;
    int operator[](int i) const { return i == 0 ? R : i == 1 ? G : i == 2 ? B : A; }
    bool operator==(const Rgba&) const = default;
};

std::ostream& operator<<(std::ostream& o, const Rgba& v) {
    return o << "[" << v.R << ", " << v.G << ", " << v.B << ", " << v.A << "]";
}

Rgba Pixel(const BakedFrame& f, int x, int y, bool normals = false) {
    const auto& b = normals ? f.Normals : f.Color;
    const std::size_t i = std::size_t(y * f.Width + x) * 4;
    return {b[i], b[i + 1], b[i + 2], b[i + 3]};
}

} // namespace

TEST(SpriteFileRoundTripsExactly) {
    SpriteFile s = Small();
    s.Tags = {{"idle", 0, 2, "pingpong", true, {{"blink", {1}}}}};
    s.Sockets = {{"hand", {{"0", {3, 2, 90}}}}};
    const std::string first = s.ToJson();
    auto back = SpriteFile::Parse(first, "test");
    CHECK(back.has_value());
    CHECK_EQ(back->ToJson(), first);
    CHECK_EQ(back->Cels[0].Indices().size(), std::size_t(2));
    // A default sprite is valid too, and missing keys take their defaults.
    CHECK(SpriteFile::Parse(SpriteFile{}.ToJson()).has_value());
    auto minimal = SpriteFile::Parse(R"({"Format": "bitsprite"})");
    CHECK(minimal.has_value());
    CHECK_EQ(minimal->Size[0], 16);
}

TEST(SpriteFileRejectsOtherFiles) {
    CHECK(!SpriteFile::Parse(R"({"Format": "bitmusic"})").has_value());
    CHECK(!SpriteFile::Parse(R"({"Format": "bitsprite", "Size": [0, 4]})").has_value());
    CHECK(!SpriteFile::Parse("{ not json").has_value());
}

TEST(SpriteCelIndicesSurviveBase64) {
    for (int n = 0; n < 7; ++n) {
        std::vector<std::uint8_t> bytes;
        for (int i = 0; i < n; ++i) bytes.push_back(std::uint8_t(i * 37 + 250));
        SpriteCel cel;
        cel.SetIndices({0, 0}, {n, 1}, bytes);
        CHECK(cel.Indices() == bytes);
    }
    SpriteCel broken;
    broken.Size = {2, 1};
    broken.Pixels = "@@@@";
    CHECK(broken.Indices().empty());
}

TEST(SpriteBakesPixelsAndLinks) {
    SpriteFile s = Small();
    BakedFrame f = s.Bake(0);
    CHECK_EQ(f.Width, 4);
    CHECK_EQ(f.Color.size(), std::size_t(64));
    CHECK_EQ(Pixel(f, 1, 1), (Rgba{255, 0, 0, 255}));
    CHECK_EQ(Pixel(f, 2, 1), (Rgba{255, 0, 0, 255}));
    CHECK_EQ(Pixel(f, 0, 0)[3], 0);
    CHECK_EQ(Pixel(f, 1, 1, true), (Rgba{128, 128, 255, 0})); // flat, no glow
    // Frame 1 is linked to frame 0; frame 2 has no cel; a frame that does not exist is empty.
    CHECK(s.Bake(1).Color == f.Color);
    CHECK_EQ(Pixel(s.Bake(2), 1, 1)[3], 0);
    CHECK_EQ(s.Bake(9).Color.size(), std::size_t(64));
}

TEST(SpriteSkipsHiddenLayers) {
    SpriteFile s = Small();
    s.Layers[0].Visible = false;
    CHECK_EQ(Pixel(s.Bake(0), 1, 1)[3], 0);
    s.Layers[0].Visible = true;
    s.Layers[0].Reference = true;
    CHECK_EQ(Pixel(s.Bake(0), 1, 1)[3], 0);
    s.Layers[0].Reference = false;
    s.Layers.insert(s.Layers.begin(), SpriteLayer{"folder", "group"});
    s.Layers[0].Visible = false;
    s.Layers[1].Parent = 0;
    for (SpriteCel& cel : s.Cels) cel.Layer = 1;
    CHECK_EQ(Pixel(s.Bake(0), 1, 1)[3], 0);
    s.Layers[0].Visible = true;
    CHECK_EQ(Pixel(s.Bake(0), 1, 1)[3], 255);
}

TEST(SpriteBakesPieces) {
    SpriteFile s = Small();
    s.Size = {12, 12};
    s.Layers = {{"body", "pieces"}};
    SpriteCel cel;
    cel.Frame = 0;
    SpritePiece box;
    box.Shape = "box";
    box.At = {6, 6};
    box.Size = {8, 8};
    box.Material = "steel";
    box.Height = 3;
    box.Bevel = 2;
    cel.Pieces.push_back(box);
    SpritePiece lamp;
    lamp.Shape = "pixel";
    lamp.At = {6, 6};
    lamp.Material = "lamp";
    lamp.Height = 3;
    lamp.Z = 1;
    cel.Pieces.push_back(lamp);
    s.Cels = {cel};
    BakedFrame f = s.Bake(0);
    // Covers 2..9; the flat middle keeps the colour exactly, the top-left edge is lit, the bottom-right dark.
    CHECK_EQ(Pixel(f, 1, 5)[3], 0);
    CHECK_EQ(Pixel(f, 2, 5)[3], 255);
    CHECK_EQ(Pixel(f, 9, 5)[3], 255);
    CHECK_EQ(Pixel(f, 10, 5)[3], 0);
    CHECK_EQ(Pixel(f, 5, 5), (Rgba{102, 102, 102, 255}));
    CHECK(Pixel(f, 2, 5)[0] > 102);
    CHECK(Pixel(f, 9, 5)[0] < 102);
    CHECK(Pixel(f, 2, 5, true)[0] < 128); // normal leans left on the left edge
    CHECK(Pixel(f, 5, 9, true)[1] > 128); // and down on the bottom edge
    // The lamp: on top (Z 1), green, glowing.
    CHECK_EQ(Pixel(f, 6, 6)[1], 255);
    CHECK_EQ(Pixel(f, 6, 6, true)[3], 255);
    // Paint recolours only what is there.
    SpritePiece paint;
    paint.Shape = "paint";
    paint.At = {0, 0};
    paint.To = {2, 2};
    paint.Material = "red";
    paint.Z = 2;
    s.Cels[0].Pieces.push_back(paint);
    f = s.Bake(0);
    CHECK_EQ(Pixel(f, 2, 2)[0], 255);
    CHECK_EQ(Pixel(f, 1, 1)[3], 0);
}

TEST(SpriteBakesOtherShapes) {
    SpriteFile s = Small();
    s.Size = {16, 16};
    s.Layers = {{"body", "pieces"}};
    SpriteCel cel;
    auto add = [&](std::string shape, Vec2 at, Vec2 size) {
        SpritePiece p;
        p.Shape = std::move(shape);
        p.At = at;
        p.Size = size;
        p.Material = "steel";
        cel.Pieces.push_back(p);
        return &cel.Pieces.back();
    };
    add("ellipsoid", {4, 4}, {6, 6});
    add("capsule", {12, 4}, {4, 8});
    SpritePiece* line = add("line", {0, 15}, {1, 1});
    line->To = {15, 12};
    SpritePiece* poly = add("polygon", {0, 0}, {1, 1});
    poly->Points = {{1, 8}, {7, 8}, {4, 12}};
    s.Cels = {cel};
    BakedFrame f = s.Bake(0);
    CHECK_EQ(Pixel(f, 4, 4)[3], 255);  // ellipsoid centre
    CHECK_EQ(Pixel(f, 1, 1)[3], 0);    // outside its corner
    CHECK_EQ(Pixel(f, 12, 1)[3], 255); // capsule end
    CHECK_EQ(Pixel(f, 0, 15)[3], 255); // line start
    CHECK_EQ(Pixel(f, 15, 12)[3], 255);
    for (int x = 0; x < 16; ++x) { // the line has a pixel in every column
        bool any = false;
        for (int y = 11; y < 16; ++y) any = any || Pixel(f, x, y)[3] == 255;
        CHECK(any);
    }
    CHECK_EQ(Pixel(f, 4, 9)[3], 255); // polygon
    CHECK_EQ(Pixel(f, 1, 11)[3], 0);
}

TEST(SpriteDrawsOutline) {
    SpriteFile s = Small();
    s.Outline = "auto";
    BakedFrame f = s.Bake(0);
    CHECK_EQ(Pixel(f, 1, 0), (Rgba{89, 0, 0, 255})); // above: 0.35 of the red
    CHECK_EQ(Pixel(f, 0, 1)[3], 255);
    CHECK_EQ(Pixel(f, 0, 0)[3], 0); // corners are not touched
    s.Outline = "steel";
    CHECK_EQ(Pixel(s.Bake(0), 3, 1), (Rgba{102, 102, 102, 255}));
}

TEST(SpritesPackIntoAnAtlas) {
    SpriteFile hero = Small();
    hero.Tags = {{"walk", 0, 2, "pingpong", true, {{"step", {0, 2}}}}};
    hero.Sockets = {{"hand", {{"0", {3, 2, 90}}}}};
    SpriteFile coin = Small();
    coin.Frames = {{100}};
    PackedAtlas a = PackSprites({{"hero", hero}, {"coin", coin}});
    PackedAtlas b = PackSprites({{"coin", coin}, {"hero", hero}});
    CHECK_EQ(a.Json, b.Json); // sorted by name: the order given does not matter
    CHECK(a.Image.Color == b.Image.Color);
    auto json = nlohmann::json::parse(a.Json);
    CHECK_EQ(json["filter"].get<std::string>(), std::string("nearest"));
    // coin: one frame, its own name. hero walk pingpong: 0 1 2 1.
    CHECK(json["sprites"].contains("coin"));
    CHECK_EQ(json["sprites"].size(), std::size_t(5));
    CHECK(json["sprites"].contains("hero_walk_3"));
    auto walk = json["animations"]["hero_walk"];
    CHECK_EQ(walk["fps"].get<double>(), 10.0);
    CHECK(walk["durations"] == nlohmann::json({1, 1, 2, 1}));
    CHECK(walk["events"]["step"] == nlohmann::json({0, 2}));
    // Frames 0, 1 (linked) and the coin are the same picture: stored once. Frame 2 is empty: another one.
    CHECK(json["sprites"]["coin"][0] == json["sprites"]["hero_walk_1"][0]);
    CHECK(json["sprites"]["coin"][1] == json["sprites"]["hero_walk_1"][1]);
    CHECK_EQ(a.Image.Width, 8); // two 4x4 pictures + padding: 8 wide, two shelves
    CHECK_EQ(a.Image.Height, 9);
    // Pivot and socket from the pivot.
    CHECK(json["sprites"]["coin"][4] == 2.0);
    CHECK(json["sockets"]["hero_walk_0"]["hand"] == nlohmann::json({1.0, -2.0, 90.0}));
    CHECK(!json["sockets"].contains("hero_walk_1"));
}
