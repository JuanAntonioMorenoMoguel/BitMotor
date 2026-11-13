#include "BitMotor/Render/SpriteFile.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

namespace BitMotor {

namespace {

constexpr int MaxCanvas = 4096;

// ---------------------------------------------------------------- base64

constexpr char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EncodeBase64(std::span<const std::uint8_t> bytes) {
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t a = bytes[i];
        const std::uint32_t b = i + 1 < bytes.size() ? bytes[i + 1] : 0;
        const std::uint32_t c = i + 2 < bytes.size() ? bytes[i + 2] : 0;
        const std::uint32_t v = (a << 16) | (b << 8) | c;
        out += Alphabet[(v >> 18) & 63];
        out += Alphabet[(v >> 12) & 63];
        out += i + 1 < bytes.size() ? Alphabet[(v >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? Alphabet[v & 63] : '=';
    }
    return out;
}

// nullopt if it is not base64.
std::optional<std::vector<std::uint8_t>> DecodeBase64(std::string_view text) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    if (text.size() % 4 != 0) return std::nullopt;
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = text[i + std::size_t(k)];
            if (c == '=' && i + 4 == text.size() && k >= 2) {
                v[k] = 0;
                ++pad;
            } else {
                if (pad > 0) return std::nullopt;
                v[k] = value(c);
                if (v[k] < 0) return std::nullopt;
            }
        }
        const std::uint32_t n = std::uint32_t(v[0] << 18 | v[1] << 12 | v[2] << 6 | v[3]);
        out.push_back(std::uint8_t(n >> 16));
        if (pad < 2) out.push_back(std::uint8_t(n >> 8));
        if (pad < 1) out.push_back(std::uint8_t(n));
    }
    return out;
}

// ---------------------------------------------------------------- checks

const std::set<std::string, std::less<>> Kinds = {"pixels", "pieces", "group"};
const std::set<std::string, std::less<>> Shapes = {"box", "ellipsoid", "capsule", "polygon", "line", "pixel", "paint"};
const std::set<std::string, std::less<>> Directions = {"forward", "reverse", "pingpong"};

// false (logged) if it is not a sprite; references to things that do not exist are only warnings.
bool Validate(const SpriteFile& sprite, std::string_view name) {
    if (sprite.Format != "bitsprite") {
        Log::Error("{}: not a BitSprite sprite (Format is \"{}\")", name, sprite.Format);
        return false;
    }
    if (sprite.Version > 1)
        Log::Warn("{}: made by a newer BitSprite (version {}); it may not look right", name, sprite.Version);
    if (sprite.Size[0] <= 0 || sprite.Size[1] <= 0 || sprite.Size[0] > MaxCanvas || sprite.Size[1] > MaxCanvas) {
        Log::Error("{}: Size must be 1..{} on each side (it is {}x{})", name, MaxCanvas, sprite.Size[0],
                   sprite.Size[1]);
        return false;
    }
    if (sprite.Filter != "nearest" && sprite.Filter != "linear")
        Log::Warn("{}: unknown Filter \"{}\" (nearest is used)", name, sprite.Filter);
    if (sprite.Density <= 0) Log::Warn("{}: Density must be more than 0 (1 is used)", name);
    if (sprite.Palette.size() > 256) Log::Warn("{}: only the first 256 palette entries can be used", name);
    std::set<std::string, std::less<>> colors;
    for (const SpriteColor& color : sprite.Palette)
        if (!colors.insert(color.Name).second) Log::Warn("{}: the palette has \"{}\" twice", name, color.Name);
    if (sprite.Outline != "none" && sprite.Outline != "auto" && !colors.contains(sprite.Outline))
        Log::Warn("{}: Outline \"{}\" is not in the palette", name, sprite.Outline);
    const int layers = int(sprite.Layers.size());
    const int frames = int(sprite.Frames.size());
    for (int i = 0; i < layers; ++i) {
        const SpriteLayer& layer = sprite.Layers[std::size_t(i)];
        if (!Kinds.contains(layer.Kind)) Log::Warn("{}: layer \"{}\" has an unknown Kind \"{}\"", name, layer.Name, layer.Kind);
        if (layer.Parent >= layers || layer.Parent == i ||
            (layer.Parent >= 0 && sprite.Layers[std::size_t(layer.Parent)].Kind != "group"))
            Log::Warn("{}: layer \"{}\": Parent {} is not a group", name, layer.Name, layer.Parent);
    }
    for (const SpriteFrame& frame : sprite.Frames)
        if (frame.Duration <= 0) Log::Warn("{}: a frame lasts {} ms (it must be more than 0)", name, frame.Duration);
    std::set<std::pair<int, int>> seen;
    for (const SpriteCel& cel : sprite.Cels) {
        if (cel.Layer < 0 || cel.Layer >= layers || cel.Frame < 0 || cel.Frame >= frames) {
            Log::Warn("{}: a cel is in layer {}, frame {}, which do not exist", name, cel.Layer, cel.Frame);
            continue;
        }
        if (!seen.insert({cel.Layer, cel.Frame}).second)
            Log::Warn("{}: layer {} has two cels in frame {} (the first one is used)", name, cel.Layer, cel.Frame);
        if (cel.Link >= 0) {
            if (cel.Link >= frames || cel.Link == cel.Frame)
                Log::Warn("{}: layer {}, frame {}: Link {} is not another frame", name, cel.Layer, cel.Frame, cel.Link);
            continue;
        }
        const std::string& kind = sprite.Layers[std::size_t(cel.Layer)].Kind;
        if (kind == "pixels" && !cel.Pixels.empty()) {
            auto bytes = DecodeBase64(cel.Pixels);
            if (cel.Size[0] < 0 || cel.Size[1] < 0 || !bytes ||
                bytes->size() != std::size_t(cel.Size[0]) * std::size_t(cel.Size[1]))
                Log::Warn("{}: layer {}, frame {}: Pixels is not {}x{} bytes of base64", name, cel.Layer, cel.Frame,
                          cel.Size[0], cel.Size[1]);
        }
        for (const SpritePiece& piece : cel.Pieces) {
            if (!Shapes.contains(piece.Shape))
                Log::Warn("{}: layer {}, frame {}: unknown Shape \"{}\"", name, cel.Layer, cel.Frame, piece.Shape);
            if (!colors.contains(piece.Material))
                Log::Warn("{}: layer {}, frame {}: Material \"{}\" is not in the palette", name, cel.Layer, cel.Frame,
                          piece.Material);
        }
        if (kind != "pieces" && !cel.Pieces.empty())
            Log::Warn("{}: layer {} is not a pieces layer; its pieces are ignored", name, cel.Layer);
    }
    for (const SpriteTag& tag : sprite.Tags) {
        if (tag.Name.empty() || tag.From < 0 || tag.To < tag.From || tag.To >= frames)
            Log::Warn("{}: tag \"{}\" needs a name and frames From <= To inside 0..{}", name, tag.Name, frames - 1);
        if (!Directions.contains(tag.Direction))
            Log::Warn("{}: tag \"{}\" has an unknown Direction \"{}\" (forward is used)", name, tag.Name, tag.Direction);
    }
    return true;
}

// ---------------------------------------------------------------- baking

struct Vec3f {
    float X, Y, Z;
};

float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
std::uint8_t Byte(float v) { return std::uint8_t(std::lround(Clamp01(v) * 255.0f)); }

float SegmentDistance(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float length2 = ab.X * ab.X + ab.Y * ab.Y;
    float t = length2 > 0 ? ((p.X - a.X) * ab.X + (p.Y - a.Y) * ab.Y) / length2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    const Vec2 d = p - (a + ab * t);
    return std::sqrt(d.X * d.X + d.Y * d.Y);
}

// One layer of one frame while it is baked: a palette index (-1 empty), a height and a glow per pixel.
struct LayerPixels {
    int Width, Height;
    std::vector<int> Index;
    std::vector<float> Lift;
    std::vector<float> Glow; // -1: the material's

    LayerPixels(int w, int h)
        : Width(w), Height(h), Index(std::size_t(w * h), -1), Lift(std::size_t(w * h), 0),
          Glow(std::size_t(w * h), -1) {}

    void Set(int x, int y, int index, float lift, float glow) {
        if (x < 0 || y < 0 || x >= Width || y >= Height) return;
        const std::size_t i = std::size_t(y * Width + x);
        Index[i] = index;
        Lift[i] = lift;
        Glow[i] = glow;
    }
};

// Height of a piece at pixel centre `p` (nullopt: outside it).
std::optional<float> PieceHeight(const SpritePiece& piece, Vec2 p) {
    const float height = std::max(0.0f, piece.Height);
    auto bevelled = [&](float edge) {
        return piece.Bevel > 0 ? height * std::min(1.0f, edge / piece.Bevel) : height;
    };
    const float hx = piece.Size.X * 0.5f, hy = piece.Size.Y * 0.5f;
    const float dx = std::fabs(p.X - piece.At.X), dy = std::fabs(p.Y - piece.At.Y);
    if (piece.Shape == "box") {
        if (dx > hx || dy > hy) return std::nullopt;
        return bevelled(std::min(hx - dx, hy - dy) + 0.5f);
    }
    if (piece.Shape == "ellipsoid") {
        if (hx <= 0 || hy <= 0) return std::nullopt;
        const float r2 = (dx / hx) * (dx / hx) + (dy / hy) * (dy / hy);
        if (r2 > 1) return std::nullopt;
        return height * std::sqrt(1 - r2);
    }
    if (piece.Shape == "capsule") {
        const float radius = std::min(hx, hy);
        if (radius <= 0) return std::nullopt;
        const Vec2 axis = hx > hy ? Vec2(hx - radius, 0) : Vec2(0, hy - radius);
        const float q = SegmentDistance(p, piece.At - axis, piece.At + axis) / radius;
        if (q > 1) return std::nullopt;
        return height * std::sqrt(1 - q * q);
    }
    if (piece.Shape == "polygon") {
        const auto& v = piece.Points;
        if (v.size() < 3) return std::nullopt;
        bool inside = false;
        float edge = 1e30f;
        for (std::size_t i = 0, j = v.size() - 1; i < v.size(); j = i++) {
            if ((v[i].Y > p.Y) != (v[j].Y > p.Y) &&
                p.X < (v[j].X - v[i].X) * (p.Y - v[i].Y) / (v[j].Y - v[i].Y) + v[i].X)
                inside = !inside;
            edge = std::min(edge, SegmentDistance(p, v[i], v[j]));
        }
        if (!inside) return std::nullopt;
        return bevelled(edge + 0.5f);
    }
    return std::nullopt;
}

// Draws one piece into the layer, in the material `index`.
void DrawPiece(LayerPixels& out, const SpritePiece& piece, int index) {
    const float glow = piece.Emit;
    const Vec2 to = piece.To == Vec2(-1, -1) ? piece.At : piece.To;
    if (piece.Shape == "pixel") {
        out.Set(int(std::floor(piece.At.X)), int(std::floor(piece.At.Y)), index, std::max(0.0f, piece.Height), glow);
        return;
    }
    if (piece.Shape == "paint") {
        const int x0 = int(std::floor(std::min(piece.At.X, to.X))), x1 = int(std::floor(std::max(piece.At.X, to.X)));
        const int y0 = int(std::floor(std::min(piece.At.Y, to.Y))), y1 = int(std::floor(std::max(piece.At.Y, to.Y)));
        for (int y = std::max(0, y0); y <= std::min(out.Height - 1, y1); ++y)
            for (int x = std::max(0, x0); x <= std::min(out.Width - 1, x1); ++x) {
                const std::size_t i = std::size_t(y * out.Width + x);
                if (out.Index[i] < 0) continue; // paint only recolours
                out.Index[i] = index;
                out.Glow[i] = glow;
            }
        return;
    }
    if (piece.Shape == "line") {
        const Vec2 a = piece.At + Vec2(0.5f, 0.5f), b = to + Vec2(0.5f, 0.5f);
        const float half = std::max(1.0f, piece.Size.X) * 0.5f;
        const int x0 = int(std::floor(std::min(a.X, b.X) - half)), x1 = int(std::ceil(std::max(a.X, b.X) + half));
        const int y0 = int(std::floor(std::min(a.Y, b.Y) - half)), y1 = int(std::ceil(std::max(a.Y, b.Y) + half));
        for (int y = std::max(0, y0); y <= std::min(out.Height - 1, y1); ++y)
            for (int x = std::max(0, x0); x <= std::min(out.Width - 1, x1); ++x)
                if (SegmentDistance(Vec2(float(x) + 0.5f, float(y) + 0.5f), a, b) <= half + 1e-4f)
                    out.Set(x, y, index, std::max(0.0f, piece.Height), glow);
        return;
    }
    // Filled shapes: test every pixel centre of the bounding box.
    float minX, maxX, minY, maxY;
    if (piece.Shape == "polygon") {
        if (piece.Points.empty()) return;
        minX = maxX = piece.Points[0].X;
        minY = maxY = piece.Points[0].Y;
        for (const Vec2& v : piece.Points) {
            minX = std::min(minX, v.X), maxX = std::max(maxX, v.X);
            minY = std::min(minY, v.Y), maxY = std::max(maxY, v.Y);
        }
    } else {
        minX = piece.At.X - piece.Size.X * 0.5f, maxX = piece.At.X + piece.Size.X * 0.5f;
        minY = piece.At.Y - piece.Size.Y * 0.5f, maxY = piece.At.Y + piece.Size.Y * 0.5f;
    }
    for (int y = std::max(0, int(std::floor(minY))); y <= std::min(out.Height - 1, int(std::ceil(maxY))); ++y)
        for (int x = std::max(0, int(std::floor(minX))); x <= std::min(out.Width - 1, int(std::ceil(maxX))); ++x)
            if (auto h = PieceHeight(piece, Vec2(float(x) + 0.5f, float(y) + 0.5f))) out.Set(x, y, index, *h, glow);
}

// Is the layer drawn? Not when it or a group it is in is hidden, or it is a reference.
bool Shown(const SpriteFile& sprite, int layer) {
    for (int guard = 0; layer >= 0 && layer < int(sprite.Layers.size()) && guard < 64; ++guard) {
        const SpriteLayer& l = sprite.Layers[std::size_t(layer)];
        if (!l.Visible || l.Reference) return false;
        layer = l.Parent;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------- SpriteCel

std::vector<std::uint8_t> SpriteCel::Indices() const {
    const std::size_t count = std::size_t(std::max(0, Size[0])) * std::size_t(std::max(0, Size[1]));
    auto bytes = DecodeBase64(Pixels);
    if (!bytes || bytes->size() != count) return {};
    return std::move(*bytes);
}

void SpriteCel::SetIndices(std::array<int, 2> at, std::array<int, 2> size, std::span<const std::uint8_t> indices) {
    At = at;
    Size = size;
    Pixels = EncodeBase64(indices);
}

// ---------------------------------------------------------------- SpriteFile

std::optional<SpriteFile> SpriteFile::Load(const std::filesystem::path& path) {
    const std::filesystem::path full = path.is_absolute() ? path : Files::AssetPath(path);
    std::optional<std::string> text = Files::Read(full);
    if (!text) {
        Log::Error("Sprite {}: cannot read the file", path.string());
        return std::nullopt;
    }
    return Parse(*text, path.string());
}

std::optional<SpriteFile> SpriteFile::Parse(std::string_view json, std::string_view name) {
    std::optional<SpriteFile> sprite = Data::FromJson<SpriteFile>(json, name);
    if (!sprite) return std::nullopt;
    if (!Validate(*sprite, name)) return std::nullopt;
    return sprite;
}

std::string SpriteFile::ToJson() const { return Data::ToJson(*this); }

bool SpriteFile::Save(const std::filesystem::path& path) const {
    const std::filesystem::path full = path.is_absolute() ? path : Files::SavePath(path);
    return Files::Write(full, ToJson());
}

const SpriteCel* SpriteFile::Cel(int layer, int frame) const {
    for (int hops = 0; hops < 2; ++hops) {
        const SpriteCel* found = nullptr;
        for (const SpriteCel& cel : Cels)
            if (cel.Layer == layer && cel.Frame == frame) {
                found = &cel;
                break;
            }
        if (!found) return nullptr;
        if (found->Link < 0) return found;
        if (found->Link == frame || hops == 1) return nullptr; // only one link is followed
        frame = found->Link;
    }
    return nullptr;
}

int SpriteFile::ColorIndex(std::string_view name) const {
    const int count = std::min<int>(256, int(Palette.size()));
    for (int i = 0; i < count; ++i)
        if (Palette[std::size_t(i)].Name == name) return i;
    return -1;
}

BakedFrame SpriteFile::Bake(int frame) const {
    BakedFrame out;
    out.Width = std::clamp(Size[0], 0, MaxCanvas);
    out.Height = std::clamp(Size[1], 0, MaxCanvas);
    const int w = out.Width, h = out.Height;
    const std::size_t count = std::size_t(w) * std::size_t(h);
    std::vector<float> rgba(count * 4, 0.0f);
    std::vector<std::uint8_t> normals(count * 4, 0);
    for (std::size_t i = 0; i < count; ++i) normals[i * 4 + 0] = 128, normals[i * 4 + 1] = 128, normals[i * 4 + 2] = 255;
    if (frame < 0 || frame >= int(Frames.size()) || count == 0) {
        out.Color.assign(count * 4, 0);
        out.Normals = std::move(normals);
        return out;
    }
    const int colors = std::min<int>(256, int(Palette.size()));
    // The baked light comes from the top left; a flat surface keeps its colour exactly.
    const Vec3f light = {-0.5f, -0.5f, 0.70710678f};

    for (int l = 0; l < int(Layers.size()); ++l) {
        const SpriteLayer& layer = Layers[std::size_t(l)];
        if (layer.Kind == "group" || !Shown(*this, l)) continue;
        const SpriteCel* cel = Cel(l, frame);
        if (!cel) continue;
        LayerPixels pixels(w, h);
        if (layer.Kind == "pixels") {
            std::vector<std::uint8_t> indices = cel->Indices();
            for (int y = 0; y < cel->Size[1] && !indices.empty(); ++y)
                for (int x = 0; x < cel->Size[0]; ++x) {
                    const int index = indices[std::size_t(y * cel->Size[0] + x)];
                    if (index > 0 && index < colors) pixels.Set(cel->At[0] + x, cel->At[1] + y, index, layer.Height, -1);
                }
        } else if (layer.Kind == "pieces") {
            std::vector<const SpritePiece*> order;
            for (const SpritePiece& piece : cel->Pieces) order.push_back(&piece);
            std::stable_sort(order.begin(), order.end(), [](auto* a, auto* b) { return a->Z < b->Z; });
            for (const SpritePiece* piece : order) {
                const int index = ColorIndex(piece->Material);
                if (index > 0) DrawPiece(pixels, *piece, index);
            }
        } else {
            continue;
        }
        // Normals from the slope of the height (outside the layer counts as 0), then shade and blend.
        auto lift = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= w || y >= h) return 0.0f;
            const std::size_t i = std::size_t(y * w + x);
            return pixels.Index[i] >= 0 ? pixels.Lift[i] : 0.0f;
        };
        const float opacity = Clamp01(layer.Opacity);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const std::size_t i = std::size_t(y * w + x);
                if (pixels.Index[i] < 0) continue;
                const SpriteColor& color = Palette[std::size_t(pixels.Index[i])];
                Vec3f n = {-(lift(x + 1, y) - lift(x - 1, y)) * 0.5f, -(lift(x, y + 1) - lift(x, y - 1)) * 0.5f, 1};
                const float length = std::sqrt(n.X * n.X + n.Y * n.Y + n.Z * n.Z);
                n = {n.X / length, n.Y / length, n.Z / length};
                const float lit = (n.X * light.X + n.Y * light.Y + n.Z * light.Z) / light.Z;
                const float shade = std::clamp(0.5f + 0.5f * lit, 0.5f, 1.3f);
                const float alpha = Clamp01(color.Color.A) * opacity;
                const float src[3] = {Clamp01(color.Color.R * shade), Clamp01(color.Color.G * shade),
                                      Clamp01(color.Color.B * shade)};
                float* dst = &rgba[i * 4];
                const float outAlpha = alpha + dst[3] * (1 - alpha);
                for (int c = 0; c < 3; ++c)
                    dst[c] = outAlpha > 0 ? (src[c] * alpha + dst[c] * dst[3] * (1 - alpha)) / outAlpha : 0;
                dst[3] = outAlpha;
                if (alpha >= 0.5f) {
                    const float glow = pixels.Glow[i] >= 0 ? pixels.Glow[i] : color.Emit;
                    normals[i * 4 + 0] = Byte(n.X * 0.5f + 0.5f);
                    normals[i * 4 + 1] = Byte(n.Y * 0.5f + 0.5f);
                    normals[i * 4 + 2] = Byte(n.Z * 0.5f + 0.5f);
                    normals[i * 4 + 3] = Byte(glow);
                }
            }
    }

    // The outline: empty pixels next to drawn ones (up, down, left, right).
    if (Outline != "none") {
        const int fixed = Outline == "auto" ? -1 : ColorIndex(Outline);
        if (Outline == "auto" || fixed >= 0) {
            const std::vector<float> before = rgba;
            const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const std::size_t i = std::size_t(y * w + x);
                    if (before[i * 4 + 3] > 0) continue;
                    int darkest = -1;
                    float darkestLuma = 2;
                    for (int k = 0; k < 4; ++k) {
                        const int nx = x + dx[k], ny = y + dy[k];
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        const std::size_t j = std::size_t(ny * w + nx);
                        if (before[j * 4 + 3] < 0.5f) continue;
                        const float luma = 0.299f * before[j * 4] + 0.587f * before[j * 4 + 1] + 0.114f * before[j * 4 + 2];
                        if (luma < darkestLuma) darkestLuma = luma, darkest = int(j);
                    }
                    if (darkest < 0) continue;
                    if (fixed >= 0) {
                        const Color& c = Palette[std::size_t(fixed)].Color;
                        rgba[i * 4] = c.R, rgba[i * 4 + 1] = c.G, rgba[i * 4 + 2] = c.B, rgba[i * 4 + 3] = c.A;
                    } else {
                        for (int c = 0; c < 3; ++c) rgba[i * 4 + std::size_t(c)] = before[std::size_t(darkest) * 4 + std::size_t(c)] * 0.35f;
                        rgba[i * 4 + 3] = 1;
                    }
                }
        }
    }

    out.Color.resize(count * 4);
    for (std::size_t i = 0; i < count * 4; ++i) out.Color[i] = Byte(rgba[i]);
    for (std::size_t i = 0; i < count; ++i)
        if (out.Color[i * 4 + 3] == 0) normals[i * 4 + 0] = 128, normals[i * 4 + 1] = 128, normals[i * 4 + 2] = 255, normals[i * 4 + 3] = 0;
    out.Normals = std::move(normals);
    return out;
}

// ---------------------------------------------------------------- packing

PackedAtlas PackSprites(const std::vector<std::pair<std::string, SpriteFile>>& sprites, int padding) {
    padding = std::max(0, padding);
    std::vector<const std::pair<std::string, SpriteFile>*> sorted;
    for (const auto& sprite : sprites) sorted.push_back(&sprite);
    std::stable_sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->first < b->first; });

    // Every frame baked once; identical ones share a place.
    struct Place {
        BakedFrame Frame;
        int X = 0, Y = 0;
    };
    std::vector<Place> places;
    std::map<std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>, int> unique;
    struct Entry {
        std::string Name;
        const SpriteFile* Sprite;
        int SourceFrame;
        int PlaceIndex;
    };
    std::vector<Entry> entries;
    nlohmann::json animations = nlohmann::json::object();
    bool allLinear = !sorted.empty();

    for (const auto* item : sorted) {
        const std::string& name = item->first;
        const SpriteFile& sprite = item->second;
        if (sprite.Filter != "linear") allLinear = false;
        const int frames = int(sprite.Frames.size());
        std::vector<int> placeOf(std::size_t(std::max(0, frames)), -1);
        auto placeFor = [&](int frame) {
            if (placeOf[std::size_t(frame)] >= 0) return placeOf[std::size_t(frame)];
            BakedFrame baked = sprite.Bake(frame);
            auto key = std::make_pair(baked.Color, baked.Normals);
            auto found = unique.find(key);
            int index;
            if (found != unique.end()) {
                index = found->second;
            } else {
                index = int(places.size());
                unique.emplace(std::move(key), index);
                places.push_back({std::move(baked)});
            }
            placeOf[std::size_t(frame)] = index;
            return index;
        };
        // Durations in ticks of the greatest common divisor of the frames' milliseconds.
        auto animation = [&](const std::string& id, const std::vector<int>& sequence, bool loop,
                             const std::map<std::string, std::vector<int>>& events, int from) {
            int step = 0;
            for (int f : sequence) step = std::gcd(step, std::max(1, sprite.Frames[std::size_t(f)].Duration));
            step = std::max(1, step);
            nlohmann::json a = {{"fps", 1000.0 / double(step)}, {"loop", loop}};
            std::vector<int> ticks;
            for (int f : sequence) ticks.push_back(std::max(1, sprite.Frames[std::size_t(f)].Duration) / step);
            if (std::any_of(ticks.begin(), ticks.end(), [](int t) { return t != 1; })) a["durations"] = ticks;
            nlohmann::json list = nlohmann::json::object();
            for (const auto& [event, at] : events) {
                std::vector<int> hits;
                for (int k = 0; k < int(sequence.size()); ++k)
                    if (std::find(at.begin(), at.end(), sequence[std::size_t(k)] - from) != at.end()) hits.push_back(k);
                if (!hits.empty()) list[event] = hits;
            }
            if (!list.empty()) a["events"] = list;
            animations[id] = a;
        };

        if (frames == 0) continue;
        bool tagged = false;
        for (const SpriteTag& tag : sprite.Tags) {
            if (tag.Name.empty() || tag.From < 0 || tag.To < tag.From || tag.To >= frames) continue;
            tagged = true;
            std::vector<int> sequence;
            if (tag.Direction == "reverse") {
                for (int f = tag.To; f >= tag.From; --f) sequence.push_back(f);
            } else {
                for (int f = tag.From; f <= tag.To; ++f) sequence.push_back(f);
                if (tag.Direction == "pingpong")
                    for (int f = tag.To - 1; f > tag.From; --f) sequence.push_back(f);
            }
            const std::string id = name + "_" + tag.Name;
            for (int k = 0; k < int(sequence.size()); ++k)
                entries.push_back({id + "_" + std::to_string(k), &sprite, sequence[std::size_t(k)],
                                   placeFor(sequence[std::size_t(k)])});
            animation(id, sequence, tag.Loop, tag.Events, tag.From);
        }
        if (!tagged) {
            if (frames == 1) {
                entries.push_back({name, &sprite, 0, placeFor(0)});
            } else {
                std::vector<int> sequence;
                for (int f = 0; f < frames; ++f) {
                    sequence.push_back(f);
                    entries.push_back({name + "_" + std::to_string(f), &sprite, f, placeFor(f)});
                }
                animation(name, sequence, true, {}, 0);
            }
        }
    }

    // Shelves: tallest first, then in order; the width is a power of two near the square root of the area.
    std::vector<int> order(places.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return places[std::size_t(a)].Frame.Height > places[std::size_t(b)].Frame.Height; });
    long long area = 0;
    int widest = 1;
    for (const Place& p : places) {
        area += (long long)(p.Frame.Width + padding) * (p.Frame.Height + padding);
        widest = std::max(widest, p.Frame.Width);
    }
    int width = 1;
    while (width < widest || (long long)width * width < area) width *= 2;
    int x = 0, y = 0, shelf = 0, height = 0;
    for (int index : order) {
        Place& p = places[std::size_t(index)];
        if (x > 0 && x + p.Frame.Width > width) {
            x = 0;
            y += shelf + padding;
            shelf = 0;
        }
        p.X = x;
        p.Y = y;
        x += p.Frame.Width + padding;
        shelf = std::max(shelf, p.Frame.Height);
        height = std::max(height, y + p.Frame.Height);
    }

    PackedAtlas atlas;
    atlas.Image.Width = places.empty() ? 0 : width;
    atlas.Image.Height = height;
    const std::size_t pixels = std::size_t(atlas.Image.Width) * std::size_t(atlas.Image.Height);
    atlas.Image.Color.assign(pixels * 4, 0);
    atlas.Image.Normals.assign(pixels * 4, 0);
    for (std::size_t i = 0; i < pixels; ++i) atlas.Image.Normals[i * 4] = 128, atlas.Image.Normals[i * 4 + 1] = 128,
                                              atlas.Image.Normals[i * 4 + 2] = 255;
    for (const Place& p : places)
        for (int row = 0; row < p.Frame.Height; ++row) {
            const std::size_t from = std::size_t(row * p.Frame.Width) * 4;
            const std::size_t to = (std::size_t(p.Y + row) * std::size_t(atlas.Image.Width) + std::size_t(p.X)) * 4;
            const std::size_t bytes = std::size_t(p.Frame.Width) * 4;
            std::copy_n(p.Frame.Color.begin() + std::ptrdiff_t(from), bytes, atlas.Image.Color.begin() + std::ptrdiff_t(to));
            std::copy_n(p.Frame.Normals.begin() + std::ptrdiff_t(from), bytes, atlas.Image.Normals.begin() + std::ptrdiff_t(to));
        }

    nlohmann::json json = nlohmann::json::object();
    json["filter"] = allLinear ? "linear" : "nearest";
    nlohmann::json list = nlohmann::json::object();
    nlohmann::json sockets = nlohmann::json::object();
    for (const Entry& e : entries) {
        const Place& p = places[std::size_t(e.PlaceIndex)];
        nlohmann::json rect = {p.X, p.Y, p.Frame.Width, p.Frame.Height, e.Sprite->Pivot.X, e.Sprite->Pivot.Y};
        if (e.Sprite->Density > 0 && e.Sprite->Density != 1) rect.push_back(e.Sprite->Density);
        list[e.Name] = rect;
        nlohmann::json points = nlohmann::json::object();
        for (const SpriteSocket& socket : e.Sprite->Sockets) {
            auto found = socket.Frames.find(std::to_string(e.SourceFrame));
            if (socket.Name.empty() || found == socket.Frames.end() || found->second.size() < 2) continue;
            nlohmann::json v = {found->second[0] - e.Sprite->Pivot.X, found->second[1] - e.Sprite->Pivot.Y};
            if (found->second.size() > 2) v.push_back(found->second[2]);
            points[socket.Name] = v;
        }
        if (!points.empty()) sockets[e.Name] = points;
    }
    json["sprites"] = list;
    if (!animations.empty()) json["animations"] = animations;
    if (!sockets.empty()) json["sockets"] = sockets;
    atlas.Json = json.dump(1, '\t') + "\n";
    return atlas;
}

} // namespace BitMotor
