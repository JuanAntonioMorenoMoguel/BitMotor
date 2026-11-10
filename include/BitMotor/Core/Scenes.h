#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Services.h"
#include "BitMotor/Core/Tasks.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// Scenes: each screen of the game (title, a level, the pause menu) is a Scene, kept in a stack.
//     struct Pause : Scene {
//         Pause() { DrawBelow = true; }   // the level stays visible behind, frozen
//         void Update() override { if (Input::Pressed(Action::Pause)) Scenes::Pop(); }
//     };
//     Scenes::Push(MakeScene<Pause>());                                     // on top; returns Pause&
//     Scenes::Replace(MakeScene<Level>(2), Transition::Fade(0.4f));         // instead of the top one
//     Scenes::Pop();  Scenes::Clear();  Scenes::Current();
//
// Only the top scene updates and draws, unless it sets UpdateBelow / DrawBelow: then the one below also
// does (and so on down while each one asks for it), bottom first. Stack changes wait until the end of the
// current Update, so they are safe from anywhere. Data reaches a scene through its constructor.
//
// Managers split a scene's work (physics, enemies, HUD). They update and draw in the order they were
// added, before the scene's own Update and Draw:
//     struct Level : Scene {
//         Level(int number) { Add<Physics>(); Add<Enemies>(number); }
//         void Draw() override { /* HUD on top of what the managers drew */ }
//     };
//     struct Enemies : Manager {
//         void Update() override { Get<Physics>().Step(); }   // another manager of the same scene
//     };
// Managers added in the scene's constructor start together, in order, just before Enter (so their Start
// can already Get the others); those added later start at once. They stop in reverse order after Exit.
namespace BitMotor {

class Scene;

namespace Detail {
struct SceneAccess;
[[noreturn]] void MissingManager(const char* name);
} // namespace Detail

class Manager {
public:
    virtual ~Manager() = default;
    // Before the scene's Enter (or right away if the scene is already running).
    virtual void Start() {}
    // Each Update while the scene updates and Active is true; before the scene's own Update.
    virtual void Update() {}
    // Each frame while the scene draws and Active is true; before the scene's own Draw.
    virtual void Draw() {}
    // After the scene's Exit, in reverse order.
    virtual void Stop() {}

    // The scene it belongs to. Valid from Start (not in the constructor).
    Scene& Owner() const { return *OwnerScene; }
    // Another manager of the same scene.
    template <class T> T& Get() const;

    // false: neither Update nor Draw run.
    bool Active = true;

private:
    friend class Scene;
    Scene* OwnerScene = nullptr;
};

class Scene {
public:
    Scene();
    virtual ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // When it becomes part of the stack (Push or Replace).
    virtual void Enter() {}
    // When it leaves the stack (Pop, Replace, Clear or the end of the game). Its tasks are cancelled.
    virtual void Exit() {}
    // When the scene above it is popped and it is on top again.
    virtual void Resume() {}
    // Each Update while it updates; after its managers, before its tasks.
    virtual void Update() {}
    // Each frame while it draws; after its managers.
    virtual void Draw() {}
    // While the debug panel is open (F1), every frame, for every scene in the stack: ImGui calls.
    virtual void DebugPanel() {}

    // The scene below also updates (a HUD scene over the level).
    bool UpdateBelow = false;
    // The scene below also draws, frozen unless UpdateBelow (a pause menu over the level).
    bool DrawBelow = false;

    // Creates a manager of this scene with `args` and returns it. One of each type.
    template <class T, class... A> T& Add(A&&... args) {
        static_assert(std::is_base_of_v<Manager, T>, "Scene::Add<T>: T must derive from BitMotor::Manager");
        auto manager = std::make_unique<T>(std::forward<A>(args)...);
        T& result = *manager;
        AddManager(Detail::KeyOf<T>(), typeid(T).name(), std::move(manager));
        return result;
    }
    // The manager, or nullptr.
    template <class T> T* Find() const {
        static_assert(std::is_base_of_v<Manager, T>, "Scene::Find<T>: T must derive from BitMotor::Manager");
        return static_cast<T*>(FindManager(Detail::KeyOf<T>()));
    }
    // The manager. If the scene does not have it the game cannot go on: logs an error and aborts.
    template <class T> T& Get() const {
        if (T* manager = Find<T>()) return *manager;
        Detail::MissingManager(typeid(T).name());
    }

    // Starts a task that belongs to this scene: it advances only while the scene updates and is cancelled
    // when the scene exits.
    Task Run(Sequence sequence);

    // It is in the stack (between Enter and Exit).
    bool Running() const { return InStack; }

private:
    friend struct Detail::SceneAccess;
    void AddManager(Detail::TypeKey key, const char* name, std::unique_ptr<Manager> manager);
    Manager* FindManager(Detail::TypeKey key) const;

    struct Entry {
        Detail::TypeKey Key;
        std::unique_ptr<Manager> Object;
    };
    std::vector<Entry> Managers;
    std::size_t StartedManagers = 0;
    Detail::TaskList Tasks;
    bool InStack = false;
};

template <class T> T& Manager::Get() const { return Owner().template Get<T>(); }

template <class T, class... A> std::unique_ptr<T> MakeScene(A&&... args) {
    static_assert(std::is_base_of_v<Scene, T>, "MakeScene<T>: T must derive from BitMotor::Scene");
    return std::make_unique<T>(std::forward<A>(args)...);
}

// How a stack change looks.
//     None:  at once.
//     Fade:  the screen goes to `color` in half the time, the change happens, and it comes back in the other half.
//     Wipe:  the same with an edge of `color` that sweeps across the screen towards a side, covering it, and
//            sweeps on towards the same side uncovering the new scene.
//     Slide: the new scene pushes the old one off the screen towards a side (the old one is the last frame
//            drawn before the change, held still).
// While the screen is covered (Fade, Wipe) or for the one frame before a slide, no scene updates.
//     Scenes::Push(MakeScene<Options>(), Transition::Slide(0.4f, Transition::Left));
//     Scenes::Replace(MakeScene<Level>(2), Transition::Wipe(0.6f, Transition::Down, Colors::Black));
struct Transition {
    enum class Style { Fade, Wipe, Slide };
    // Where a Wipe's edge and a Slide's scenes move to.
    enum Side { Left, Right, Up, Down };

    float Seconds = 0;
    Color Tint = Colors::Black;
    Style Look = Style::Fade;
    Side Towards = Left;

    static Transition None() { return {}; }
    static Transition Fade(float seconds = 0.4f, Color color = Colors::Black) { return {seconds, color}; }
    static Transition Wipe(float seconds = 0.6f, Side towards = Right, Color color = Colors::Black) {
        return {seconds, color, Style::Wipe, towards};
    }
    static Transition Slide(float seconds = 0.4f, Side towards = Left) {
        return {seconds, Colors::Black, Style::Slide, towards};
    }
};

// Emitted after the stack changes.
struct SceneChanged {
    Scene* Current = nullptr; // the top scene now (nullptr: empty stack)
};

namespace Detail {
enum class SceneOp { Push, Pop, Replace, Clear };
Scene& RequestScene(SceneOp op, std::unique_ptr<Scene> scene, Transition transition);
void RequestScene(SceneOp op, Transition transition);
} // namespace Detail

namespace Scenes {

// Puts `scene` on top. Returns it, so the caller can keep a reference (valid until it is popped).
template <class T> T& Push(std::unique_ptr<T> scene, Transition transition = {}) {
    return static_cast<T&>(Detail::RequestScene(Detail::SceneOp::Push, std::move(scene), transition));
}
// Takes the top scene out and puts `scene` in its place (the one below does not Resume).
template <class T> T& Replace(std::unique_ptr<T> scene, Transition transition = {}) {
    return static_cast<T&>(Detail::RequestScene(Detail::SceneOp::Replace, std::move(scene), transition));
}
// Takes the top scene out; the one below gets Resume.
inline void Pop(Transition transition = {}) { Detail::RequestScene(Detail::SceneOp::Pop, transition); }
// Takes every scene out, top first.
inline void Clear(Transition transition = {}) { Detail::RequestScene(Detail::SceneOp::Clear, transition); }

// The top scene of the stack (changes requested in this Update are not there yet). nullptr if empty.
Scene* Current();
// Scenes in the stack.
std::size_t Count();
// A fade is in progress.
bool InTransition();

} // namespace Scenes

} // namespace BitMotor
