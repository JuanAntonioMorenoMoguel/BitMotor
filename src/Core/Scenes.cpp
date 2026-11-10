#include "BitMotor/Core/Scenes.h"

#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Time.h"
#include "Core/ScenesInternal.h"
#include "Render/RendererInternal.h"

#include <cstdlib>

namespace BitMotor {

namespace Detail {

void MissingManager(const char* name) {
    Log::Error("Scene::Get<{}>: this scene has no such manager (Add<{}> in its constructor)", name, name);
    std::abort();
}

// What the stack does to a scene, in one place.
struct SceneAccess {
    static void Enter(Scene& scene) {
        scene.InStack = true;
        StartManagers(scene);
        scene.Enter();
    }

    static void Exit(Scene& scene) {
        scene.Exit();
        scene.Tasks.Clear();
        while (scene.StartedManagers > 0) {
            --scene.StartedManagers;
            if (Manager* manager = scene.Managers[scene.StartedManagers].Object.get()) manager->Stop();
        }
        scene.InStack = false;
    }

    // Managers not started yet, in order. One added from a Start is started by AddManager itself.
    static void StartManagers(Scene& scene) {
        while (scene.StartedManagers < scene.Managers.size()) scene.Managers[scene.StartedManagers++].Object->Start();
    }

    static void Update(Scene& scene) {
        // By index: a manager may add another one, which updates from the next Update.
        const std::size_t count = scene.Managers.size();
        for (std::size_t i = 0; i < count; ++i)
            if (Manager* manager = scene.Managers[i].Object.get(); manager->Active) manager->Update();
        scene.Update();
        scene.Tasks.Update(Time::Delta());
    }

    static void Draw(Scene& scene) {
        const std::size_t count = scene.Managers.size();
        for (std::size_t i = 0; i < count; ++i)
            if (Manager* manager = scene.Managers[i].Object.get(); manager->Active) manager->Draw();
        scene.Draw();
    }

    static bool UpdatesBelow(const Scene& scene) { return scene.UpdateBelow; }
    static bool DrawsBelow(const Scene& scene) { return scene.DrawBelow; }
};

namespace {

struct Request {
    SceneOp Op = SceneOp::Push;
    std::unique_ptr<Scene> Next;
    Transition Look;
};

enum class Fade { None, Out, In };

std::vector<std::unique_ptr<Scene>> Stack;
std::vector<Request> Pending;
Fade Phase = Fade::None;
float FadeTime = 0, FadeHalf = 0;
Color FadeTint = Colors::Black;
Transition::Style FadeLook = Transition::Style::Fade;
Transition::Side FadeSide = Transition::Left;
// A Slide waits for one frame of the old scene to be drawn (the renderer keeps it), then changes.
bool SlideDrawn = false;
float SlideSeconds = 0;

void PopTop() {
    std::unique_ptr<Scene> top = std::move(Stack.back());
    Stack.pop_back();
    SceneAccess::Exit(*top);
}

void Apply(Request& request) {
    switch (request.Op) {
    case SceneOp::Push:
        Stack.push_back(std::move(request.Next));
        SceneAccess::Enter(*Stack.back());
        break;
    case SceneOp::Replace:
        if (!Stack.empty()) PopTop();
        Stack.push_back(std::move(request.Next));
        SceneAccess::Enter(*Stack.back());
        break;
    case SceneOp::Pop:
        if (Stack.empty()) {
            Log::Warn("Scenes::Pop: the stack is already empty");
            break;
        }
        PopTop();
        if (!Stack.empty()) Stack.back()->Resume();
        break;
    case SceneOp::Clear:
        while (!Stack.empty()) PopTop();
        break;
    }
}

void StartFadeOut(const Transition& look) {
    // From a fade coming in, it turns round from the current darkness instead of jumping (a wipe or a
    // slide starts over).
    const bool turn = Phase == Fade::In && FadeHalf > 0 && FadeLook == Transition::Style::Fade &&
                      look.Look == Transition::Style::Fade;
    const float alpha = turn ? 1.0f - FadeTime / FadeHalf : 0.0f;
    Phase = Fade::Out;
    FadeLook = look.Look;
    FadeSide = look.Towards;
    FadeTint = look.Tint;
    SlideDrawn = false;
    // A slide has no covering half: it is all movement, after the change.
    FadeHalf = look.Look == Transition::Style::Slide ? 0.0f : look.Seconds * 0.5f;
    FadeTime = alpha * FadeHalf;
    if (look.Look == Transition::Style::Slide) SlideSeconds = look.Seconds;
}

// Applies the pending requests in order. A request with a fade stops there and starts fading out, unless
// `throughFades` (the middle of a fade: everything waiting goes in at once).
void ApplyPending(bool throughFades) {
    bool changed = false;
    int guard = 0;
    while (!Pending.empty()) {
        if (!throughFades && Pending.front().Look.Seconds > 0) {
            StartFadeOut(Pending.front().Look);
            break;
        }
        Request request = std::move(Pending.front());
        Pending.erase(Pending.begin());
        Apply(request);
        changed = true;
        if (++guard > 1000) { // scenes that keep pushing each other from Enter
            Log::Error("Scenes: more than 1000 changes in a row; the rest are dropped");
            Pending.clear();
        }
    }
    if (changed) Events::Emit(SceneChanged{Scenes::Current()});
}

// The lowest scene that takes part: the top one and those below it while each one above asks for it.
template <class Asks> std::size_t Lowest(Asks asks) {
    std::size_t i = Stack.size() - 1;
    while (i > 0 && asks(*Stack[i])) --i;
    return i;
}

} // namespace

Scene& RequestScene(SceneOp op, std::unique_ptr<Scene> scene, Transition transition) {
    if (!scene) {
        Log::Error("Scenes: Push or Replace with no scene (use MakeScene<T>(...))");
        std::abort();
    }
    Scene& result = *scene;
    Pending.push_back({op, std::move(scene), transition});
    return result;
}

void RequestScene(SceneOp op, Transition transition) { Pending.push_back({op, nullptr, transition}); }

void ApplySceneChanges() {
    if (Phase != Fade::Out) ApplyPending(false);
}

void UpdateScenes() {
    const float dt = Time::Delta();
    if (Phase == Fade::Out) {
        if (FadeLook == Transition::Style::Slide) {
            if (!SlideDrawn) return; // the old scene still has to be drawn once
        } else {
            FadeTime += dt;
            if (FadeTime < FadeHalf) return; // nothing updates while the screen is covered
        }
        ApplyPending(true);
        Phase = Fade::In;
        FadeTime = 0;
        if (FadeLook == Transition::Style::Slide) FadeHalf = SlideSeconds;
        return;
    }
    if (Phase == Fade::In) {
        FadeTime += dt;
        if (FadeTime >= FadeHalf) Phase = Fade::None;
    }
    if (!Stack.empty()) {
        // The stack itself only changes in ApplyPending, so indexes stay valid here.
        for (std::size_t i = Lowest(SceneAccess::UpdatesBelow); i < Stack.size(); ++i) SceneAccess::Update(*Stack[i]);
    }
    ApplyPending(false);
}

void DrawScenes() {
    if (!Stack.empty())
        for (std::size_t i = Lowest(SceneAccess::DrawsBelow); i < Stack.size(); ++i) SceneAccess::Draw(*Stack[i]);
    if (Phase == Fade::Out && FadeLook == Transition::Style::Slide) SlideDrawn = true;
    const float amount = SceneFade();
    if (amount <= 0) return;
    if (FadeLook == Transition::Style::Fade) {
        FillScreen(FadeTint.WithAlpha(FadeTint.A * amount));
        return;
    }
    // A wipe: covering from the side it leaves while going out, uncovering towards the side it goes to.
    const Vec2 size = Screen::Size();
    const bool out = Phase == Fade::Out;
    Rect area(0, 0, size.X, size.Y);
    switch (FadeSide) {
    case Transition::Right:
        area.Width = size.X * amount;
        area.X = out ? 0 : size.X - area.Width;
        break;
    case Transition::Left:
        area.Width = size.X * amount;
        area.X = out ? size.X - area.Width : 0;
        break;
    case Transition::Down:
        area.Height = size.Y * amount;
        area.Y = out ? 0 : size.Y - area.Height;
        break;
    case Transition::Up:
        area.Height = size.Y * amount;
        area.Y = out ? size.Y - area.Height : 0;
        break;
    }
    FillArea(area, FadeTint);
}

void DebugPanelScenes() {
    for (std::size_t i = 0; i < Stack.size(); ++i) Stack[i]->DebugPanel();
}

float SceneFade() {
    if (Phase == Fade::None || FadeHalf <= 0 || FadeLook == Transition::Style::Slide) return 0;
    float t = Clamp(FadeTime / FadeHalf, 0.0f, 1.0f);
    return Phase == Fade::Out ? t : 1.0f - t;
}

bool SceneSlide(float& progress, int& towards) {
    if (Phase != Fade::In || FadeLook != Transition::Style::Slide || FadeHalf <= 0) return false;
    const float t = Clamp(FadeTime / FadeHalf, 0.0f, 1.0f);
    progress = t * t * (3 - 2 * t); // eased in and out
    towards = int(FadeSide);
    return true;
}

bool SceneSnapshotWanted() { return Phase == Fade::Out && FadeLook == Transition::Style::Slide && SlideDrawn; }

void ShutdownScenes() {
    Pending.clear();
    while (!Stack.empty()) PopTop();
    Pending.clear(); // anything an Exit asked for
    Phase = Fade::None;
    FadeTime = FadeHalf = 0;
    FadeLook = Transition::Style::Fade;
    SlideDrawn = false;
}

} // namespace Detail

// ------------------------------------------------------------------------------------------------- Scene

Scene::Scene() = default;
Scene::~Scene() = default;

void Scene::AddManager(Detail::TypeKey key, const char* name, std::unique_ptr<Manager> manager) {
    if (FindManager(key)) {
        Log::Error("Scene::Add<{}>: this scene already has one", name);
        std::abort();
    }
    manager->OwnerScene = this;
    Managers.push_back({key, std::move(manager)});
    // Already running and every earlier manager started: this one starts now.
    if (InStack && StartedManagers + 1 == Managers.size()) {
        ++StartedManagers;
        Managers.back().Object->Start();
    }
}

Manager* Scene::FindManager(Detail::TypeKey key) const {
    for (const Entry& entry : Managers)
        if (entry.Key == key) return entry.Object.get();
    return nullptr;
}

Task Scene::Run(Sequence sequence) { return Tasks.Run(std::move(sequence)); }

// ------------------------------------------------------------------------------------------------ Scenes

namespace Scenes {

Scene* Current() { return Detail::Stack.empty() ? nullptr : Detail::Stack.back().get(); }
std::size_t Count() { return Detail::Stack.size(); }
bool InTransition() { return Detail::Phase != Detail::Fade::None; }

} // namespace Scenes

} // namespace BitMotor
