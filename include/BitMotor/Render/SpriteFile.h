#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Core/Data.h"
#include "BitMotor/Math/Vector.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A .bitsprite file: one sprite as BitSprite edits it (layers of pixels and of pieces, frames, cels, tags), the
// format in docs/BitSprite.md. Games usually do not touch it: Atlas::Load("hero.bitsprite") bakes and packs it,
// and `bitsprite pack` writes the atlas.json + atlas.png + atlas_n.png that Atlas::Load reads in a release.
//     std::optional<SpriteFile> hero = SpriteFile::Load("sprites/hero.bitsprite");
//     BakedFrame first = hero->Bake(0);                       // colour + normals of frame 0
//     PackedAtlas atlas = PackSprites({{"hero", *hero}});      // atlas.json text and its two images
// Everything here is deterministic: the same file gives the same bytes on every machine.
namespace BitMotor {

// One entry of the indexed palette, which is also a material: pixels store its index and pieces name it.
// Index 0 is always transparent, whatever it says.
struct SpriteColor {
    std::string Name;
    BitMotor::Color Color = BitMotor::Color(0, 0, 0, 1);
    // 0..1: how much it glows by itself, without lights (the alpha of atlas_n.png).
    float Emit = 0;
};
BITMOTOR_FIELDS(SpriteColor, Name, Color, Emit)

struct SpriteLayer {
    std::string Name;
    // "pixels", "pieces" or "group" (a folder: layers inside it name it in Parent).
    std::string Kind = "pixels";
    // Index of the group this layer is in; -1 at the top level.
    int Parent = -1;
    bool Visible = true;
    float Opacity = 1;
    // Only for the editor: the layer cannot be changed.
    bool Locked = false;
    // A pixels layer as a flat height, in pixels, above what is under it (for the normals of later pieces).
    float Height = 0;
    // A reference image (a sketch to trace over): drawn in the editor, never baked.
    bool Reference = false;
};
BITMOTOR_FIELDS(SpriteLayer, Name, Kind, Parent, Visible, Opacity, Locked, Height, Reference)

struct SpriteFrame {
    // Milliseconds.
    int Duration = 100;
};
BITMOTOR_FIELDS(SpriteFrame, Duration)

// A procedural shape: it fills its outline with a material and raises it `Height` pixels, which gives the
// shading and the normals. Coordinates are canvas pixels (0,0 the top-left corner, Y down).
struct SpritePiece {
    // "box", "ellipsoid", "capsule", "polygon", "line", "pixel" or "paint" (recolours what is already there).
    std::string Shape = "box";
    // The centre (box, ellipsoid, capsule), the start (line, paint) or the pixel.
    Vec2 At = {0, 0};
    // The end of line and paint; {-1, -1} means the same as At.
    Vec2 To = {-1, -1};
    // Width and height (box, ellipsoid, capsule); line: X is the thickness.
    Vec2 Size = {1, 1};
    // Corners of a polygon.
    std::vector<Vec2> Points;
    // A palette name.
    std::string Material;
    float Height = 1;
    // Pixels from the edge over which the height rises (0: a flat top with sharp edges).
    float Bevel = 0;
    // Order inside the cel: higher covers lower. Equal Z keeps the list order.
    int Z = 0;
    // Overrides the material's Emit when 0 or more.
    float Emit = -1;
};
BITMOTOR_FIELDS(SpritePiece, Shape, At, To, Size, Points, Material, Height, Bevel, Z, Emit)

// What one layer has in one frame. A layer with no cel in a frame is empty there.
struct SpriteCel {
    int Layer = 0;
    int Frame = 0;
    // Another frame of the same layer whose cel this one shows (a linked cel); -1 when it has its own.
    int Link = -1;
    // Pixels layer: the rectangle it covers and its palette indices, one byte each, row by row, in base64.
    std::array<int, 2> At = {0, 0};
    std::array<int, 2> Size = {0, 0};
    std::string Pixels;
    // Pieces layer.
    std::vector<SpritePiece> Pieces;

    // The indices of Pixels (Size[0] * Size[1] of them; empty if they do not fit).
    std::vector<std::uint8_t> Indices() const;
    void SetIndices(std::array<int, 2> at, std::array<int, 2> size, std::span<const std::uint8_t> indices);
};
BITMOTOR_FIELDS(SpriteCel, Layer, Frame, Link, At, Size, Pixels, Pieces)

// An animation: frames From..To (both included) of the file.
struct SpriteTag {
    std::string Name;
    int From = 0, To = 0;
    // "forward", "reverse" or "pingpong".
    std::string Direction = "forward";
    bool Loop = true;
    // Events by name, at frames counted from From.
    std::map<std::string, std::vector<int>> Events;
};
BITMOTOR_FIELDS(SpriteTag, Name, From, To, Direction, Loop, Events)

// An attachment point (Atlas::Socket): [x, y] or [x, y, degrees] in canvas pixels, per frame ("0", "1"...).
struct SpriteSocket {
    std::string Name;
    std::map<std::string, std::vector<float>> Frames;
};
BITMOTOR_FIELDS(SpriteSocket, Name, Frames)

// A frame with every layer baked: straight-alpha RGBA rows from the top, and the normal map (xyz in RGB as
// 128 + 127 * n, alpha: the glow), both Width * Height * 4 bytes.
struct BakedFrame {
    int Width = 0, Height = 0;
    std::vector<std::uint8_t> Color;
    std::vector<std::uint8_t> Normals;
};

struct SpriteFile {
    std::string Format = "bitsprite";
    int Version = 1;
    // The canvas, the same for every frame.
    std::array<int, 2> Size = {16, 16};
    // What goes on Sprite2D::Position, in canvas pixels.
    Vec2 Pivot = {8, 16};
    // "nearest" or "linear".
    std::string Filter = "nearest";
    // "none", "auto" (a dark line around, from the colour next to it) or a palette name.
    std::string Outline = "none";
    // Canvas pixels per screen pixel (art drawn at double resolution: 2).
    float Density = 1;
    std::vector<SpriteColor> Palette;
    std::vector<SpriteLayer> Layers;
    std::vector<SpriteFrame> Frames;
    std::vector<SpriteCel> Cels;
    std::vector<SpriteTag> Tags;
    std::vector<SpriteSocket> Sockets;

    // Reads a .bitsprite (relative paths: Files::AssetPath). nullopt if it is not one (logged); what it refers
    // to that does not exist is only a warning, and baking skips it.
    static std::optional<SpriteFile> Load(const std::filesystem::path& path);
    static std::optional<SpriteFile> Parse(std::string_view json, std::string_view name = "sprite");
    // Writes it as JSON with a fixed key order (relative paths: Files::SavePath).
    bool Save(const std::filesystem::path& path) const;
    std::string ToJson() const;

    // The cel of `layer` in `frame`, following links; nullptr if it is empty there.
    const SpriteCel* Cel(int layer, int frame) const;
    // A palette index by name; -1 if there is none.
    int ColorIndex(std::string_view name) const;
    // Every visible, non-reference layer of `frame`, bottom (Layers[0]) to top, with the outline.
    BakedFrame Bake(int frame) const;
};
BITMOTOR_FIELDS(SpriteFile, Format, Version, Size, Pivot, Filter, Outline, Density, Palette, Layers, Frames, Cels,
                Tags, Sockets)

// An atlas made from sprites: the text of atlas.json (the format Atlas reads) and its images.
struct PackedAtlas {
    std::string Json;
    BakedFrame Image; // Color = atlas.png, Normals = atlas_n.png
};

// Bakes every frame and packs them, sorted by name, with `padding` empty pixels between them. Identical frames
// are stored once. Names in the atlas: a sprite without tags is "<name>" (one frame) or "<name>_0".. with an
// animation "<name>"; each tag gives "<name>_<tag>_0".. and the animation "<name>_<tag>".
PackedAtlas PackSprites(const std::vector<std::pair<std::string, SpriteFile>>& sprites, int padding = 1);

} // namespace BitMotor
