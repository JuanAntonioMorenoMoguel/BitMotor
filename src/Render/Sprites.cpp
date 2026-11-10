#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Settings.h"
#include "BitMotor/Core/Time.h"
#include "BitMotor/Math/Functions.h"
#include "BitMotor/Math/Noise.h"
#include "BitMotor/Render/RenderTexture.h"
#include "Render/RendererInternal.h"
#include "Render/SpritesInternal.h"

#include <algorithm>
#include <numeric>

namespace BitMotor {

namespace {

Camera2D DefaultCamera() {
    Camera2D camera;
    camera.Position = Detail::ViewCenter();
    return camera;
}

Camera2D Active;
std::vector<Detail::QueuedSprite> Queue;

// RenderTextures: the one being drawn into (its draw is the last of Targets), and what the screen had then.
std::vector<Detail::TargetDraw> Targets;
bool Inside = false;
std::vector<Detail::QueuedSprite> ScreenQueue;
Camera2D ScreenCamera;

float ShakeAmount(const Camera2D& camera) {
    float trauma = Clamp(camera.Trauma, 0.0f, 1.0f);
    return trauma * trauma * Max(Settings::Get().ScreenShake, 0.0f);
}

} // namespace

void Camera2D::AddShake(float amount) { Trauma = Clamp(Trauma + amount, 0.0f, 1.0f); }

void Camera2D::Update() {
    Trauma = Max(0.0f, Trauma - ShakeDecay * Time::Delta());
    ShakeTime += ShakeSpeed * Time::Delta();
    if (Trauma <= 0) ShakeTime = 0;
}

Vec2 Camera2D::ShakeOffset() const {
    float amount = ShakeAmount(*this);
    if (amount <= 0) return {0, 0};
    return Vec2(Noise1D(ShakeTime, 1), Noise1D(ShakeTime, 2)) * (MaxShake * amount);
}

float Camera2D::ShakeAngle() const {
    float amount = ShakeAmount(*this);
    return amount <= 0 ? 0.0f : Noise1D(ShakeTime, 3) * MaxShakeAngle * amount;
}

Vec2 Camera2D::ToScreen(Vec2 world) const {
    Vec2 relative = Rotate(world - Position, -(Rotation + ShakeAngle())) * Zoom;
    return relative + Detail::ViewCenter() + ShakeOffset();
}

Vec2 Camera2D::ToWorld(Vec2 screen) const {
    Vec2 relative = screen - Detail::ViewCenter() - ShakeOffset();
    float zoom = Zoom != 0 ? Zoom : 1.0f;
    return Rotate(relative * (1.0f / zoom), Rotation + ShakeAngle()) + Position;
}

void SetCamera(const Camera2D& camera) { Active = camera; }
const Camera2D& CurrentCamera() { return Active; }

void Screen::Draw(const Sprite2D& sprite) {
    if (!sprite.Region) return;
    Detail::QueuedSprite queued;
    queued.Instance = Detail::MakeInstance(sprite, sprite.Layer == Layer::Hud ? nullptr : &Active);
    queued.Source = sprite.Region.Source;
    queued.Where = sprite.Layer;
    queued.Depth = sprite.Depth;
    Queue.push_back(std::move(queued));
}

RenderTexture RenderTexture::Create(int width, int height, Filter filter) {
    RenderTexture texture;
    if (width <= 0 || height <= 0) {
        Log::Error("RenderTexture::Create: {}x{} is empty", width, height);
        return texture;
    }
    texture.Image.Data = std::make_shared<Detail::TextureData>();
    texture.Image.Data->Width = width;
    texture.Image.Data->Height = height;
    texture.Image.Data->Look = filter;
    texture.Image.Data->Target = true;
    Detail::GpuTexture(*texture.Image.Data); // now if there is a device; otherwise when it starts
    return texture;
}

void RenderTexture::Begin(Color clear) {
    if (Image) Detail::BeginTargetDraw(Image, true, clear);
}

void RenderTexture::Continue() {
    if (Image) Detail::BeginTargetDraw(Image, false, Colors::Transparent);
}

void RenderTexture::End() {
    if (Detail::InTargetDraw() && Detail::TargetDraws().back().Target == Image)
        Detail::EndTargetDraw();
    else if (Image)
        Log::Warn("RenderTexture::End without its Begin");
}

namespace Detail {

SpriteInstance MakeInstance(const Sprite2D& sprite, const Camera2D* camera) {
    const Region& region = sprite.Region;
    SpriteInstance s;
    Vec2 size = region.Size() * sprite.Scale;
    Vec2 position = sprite.Position;
    float rotation = sprite.Rotation;
    if (camera) {
        position = camera->ToScreen(position);
        size = size * camera->Zoom;
        rotation -= camera->Rotation + camera->ShakeAngle();
    }
    s.X = position.X;
    s.Y = position.Y;
    s.Width = sprite.Flip ? -size.X : size.X;
    s.Height = size.Y;
    s.U = region.Area.X;
    s.V = region.Area.Y;
    s.UWidth = region.Area.Width;
    s.VHeight = region.Area.Height;
    s.R = sprite.Color.R;
    s.G = sprite.Color.G;
    s.B = sprite.Color.B;
    s.A = sprite.Color.A;
    s.Rotation = rotation;
    s.Flash = Clamp(sprite.Flash, 0.0f, 1.0f);
    if (sprite.Origin) {
        s.PivotX = sprite.Origin->X;
        s.PivotY = sprite.Origin->Y;
    } else {
        s.PivotX = region.Area.Width > 0 ? region.Pivot.X / region.Area.Width : 0;
        s.PivotY = region.Area.Height > 0 ? region.Pivot.Y / region.Area.Height : 0;
    }
    return s;
}

std::vector<QueuedSprite>& SpriteQueue() { return Queue; }

void ResetSprites() {
    Inside = false;
    Targets.clear();
    ScreenQueue.clear();
    Queue.clear();
    LightQueue().clear();
    Active = DefaultCamera();
}

std::vector<TargetDraw>& TargetDraws() { return Targets; }

bool InTargetDraw() { return Inside; }

Vec2 ViewCenter() { return Inside ? Targets.back().Target.Size() * 0.5f : Screen::Center(); }

bool BeginTargetDraw(const Texture& target, bool clear, Color color) {
    if (Inside) {
        Log::Error("RenderTexture::Begin: another one is still open (End it first)");
        return false;
    }
    ScreenQueue = std::move(Queue);
    Queue.clear();
    ScreenCamera = Active;
    Inside = true;
    Targets.push_back({target, clear, color, {}, {}});
    Active = DefaultCamera();
    return true;
}

void EndTargetDraw() {
    if (!Inside) return;
    Targets.back().Queue = std::move(Queue);
    Queue = std::move(ScreenQueue);
    ScreenQueue.clear();
    Active = ScreenCamera;
    Inside = false;
}

void ClearTargetDraw(Color color) {
    if (!Inside) return;
    Queue.clear();
    Targets.back().Clear = true;
    Targets.back().ClearColor = color;
}

void BuildSprites(const std::vector<QueuedSprite>& queue, std::vector<SpriteInstance>& instances,
                  std::vector<SpriteBatch>& batches) {
    instances.clear();
    batches.clear();
    std::vector<std::size_t> order(queue.size());
    std::iota(order.begin(), order.end(), std::size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const QueuedSprite &x = queue[a], &y = queue[b];
        if (x.Where != y.Where) return int(x.Where) < int(y.Where);
        return x.Where == Layer::World && x.Depth < y.Depth;
    });
    instances.reserve(queue.size());
    for (std::size_t index : order) {
        const QueuedSprite& sprite = queue[index];
        if (batches.empty() || !(batches.back().Source == sprite.Source) || batches.back().Where != sprite.Where)
            batches.push_back({sprite.Source, sprite.Where, std::uint32_t(instances.size()), 0});
        ++batches.back().Count;
        instances.push_back(sprite.Instance);
    }
}

} // namespace Detail

} // namespace BitMotor
