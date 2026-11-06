#include "Testing.h"

#include "Core/ScenesInternal.h"
#include "Core/TimeInternal.h"

#include <BitMotor/BitMotor.h>

#include <string>
#include <vector>

using namespace BitMotor;

namespace {

std::vector<std::string> Calls;

void Tick() {
    Detail::BeginUpdate();
    Detail::UpdateScenes();
}

void Ticks(int count) {
    for (int i = 0; i < count; ++i) Tick();
}

void Clean() {
    Detail::ShutdownScenes();
    Detail::ResetTime(60);
    Calls.clear();
}

struct Physics : Manager {
    int Steps = 0;
    void Start() override { Calls.push_back("Physics.Start"); }
    void Update() override {
        ++Steps;
        Calls.push_back("Physics.Update");
    }
    void Draw() override { Calls.push_back("Physics.Draw"); }
    void Stop() override { Calls.push_back("Physics.Stop"); }
};

struct Enemies : Manager {
    int SeenSteps = -1;
    void Start() override {
        Calls.push_back("Enemies.Start");
        SeenSteps = Get<Physics>().Steps; // the other manager is there already
    }
    void Update() override { Calls.push_back("Enemies.Update"); }
    void Stop() override { Calls.push_back("Enemies.Stop"); }
};

struct Unused : Manager {};

struct Named : Scene {
    std::string Name;
    explicit Named(std::string name) : Name(std::move(name)) {}
    void Enter() override { Calls.push_back(Name + ".Enter"); }
    void Exit() override { Calls.push_back(Name + ".Exit"); }
    void Resume() override { Calls.push_back(Name + ".Resume"); }
    void Update() override { Calls.push_back(Name + ".Update"); }
    void Draw() override { Calls.push_back(Name + ".Draw"); }
};

struct Level : Named {
    Level() : Named("Level") {
        Add<Physics>();
        Add<Enemies>();
    }
};

struct Overlay : Named {
    Overlay(std::string name, bool updateBelow, bool drawBelow) : Named(std::move(name)) {
        UpdateBelow = updateBelow;
        DrawBelow = drawBelow;
    }
};

} // namespace

TEST(ScenesPushEntersAtTheEndOfTheUpdate) {
    Clean();
    Level& level = Scenes::Push(MakeScene<Level>());
    CHECK(Scenes::Current() == nullptr); // not yet: changes wait
    CHECK(Calls.empty());
    Detail::ApplySceneChanges();
    CHECK_EQ(Scenes::Current(), static_cast<Scene*>(&level));
    CHECK(level.Running());
    CHECK(Calls == std::vector<std::string>({"Physics.Start", "Enemies.Start", "Level.Enter"}));
    CHECK_EQ(level.Get<Enemies>().SeenSteps, 0);
    Clean();
}

TEST(ScenesManagersUpdateAndDrawBeforeTheScene) {
    Clean();
    Scenes::Push(MakeScene<Level>());
    Detail::ApplySceneChanges();
    Calls.clear();
    Tick();
    CHECK(Calls == std::vector<std::string>({"Physics.Update", "Enemies.Update", "Level.Update"}));
    Calls.clear();
    Detail::DrawScenes();
    CHECK(Calls == std::vector<std::string>({"Physics.Draw", "Level.Draw"}));

    Scenes::Current()->Get<Physics>().Active = false;
    Calls.clear();
    Tick();
    CHECK(Calls == std::vector<std::string>({"Enemies.Update", "Level.Update"}));

    Calls.clear();
    Detail::ShutdownScenes();
    CHECK(Calls == std::vector<std::string>({"Level.Exit", "Enemies.Stop", "Physics.Stop"}));
    Clean();
}

TEST(ScenesOnlyTheTopUpdatesUnlessItAsks) {
    Clean();
    Scenes::Push(MakeScene<Named>("Game"));
    Scenes::Push(MakeScene<Overlay>("Pause", false, true));
    Detail::ApplySceneChanges();
    CHECK_EQ(Scenes::Count(), std::size_t(2));
    Calls.clear();
    Tick();
    CHECK(Calls == std::vector<std::string>({"Pause.Update"})); // the game below is frozen
    Calls.clear();
    Detail::DrawScenes();
    CHECK(Calls == std::vector<std::string>({"Game.Draw", "Pause.Draw"})); // but visible, below first

    Scenes::Push(MakeScene<Overlay>("Hud", true, false));
    Tick();
    Calls.clear();
    Tick();
    CHECK(Calls == std::vector<std::string>({"Pause.Update", "Hud.Update"})); // Pause does not ask further
    Calls.clear();
    Detail::DrawScenes();
    CHECK(Calls == std::vector<std::string>({"Hud.Draw"}));
    Clean();
}

TEST(ScenesPopResumesAndReplaceDoesNot) {
    Clean();
    Scenes::Push(MakeScene<Named>("Game"));
    Scenes::Push(MakeScene<Named>("Pause"));
    Detail::ApplySceneChanges();
    Calls.clear();
    Scenes::Pop();
    Detail::ApplySceneChanges();
    CHECK(Calls == std::vector<std::string>({"Pause.Exit", "Game.Resume"}));

    Calls.clear();
    Scenes::Replace(MakeScene<Named>("Title"));
    Detail::ApplySceneChanges();
    CHECK(Calls == std::vector<std::string>({"Game.Exit", "Title.Enter"}));
    CHECK_EQ(Scenes::Count(), std::size_t(1));

    Calls.clear();
    Scenes::Push(MakeScene<Named>("A"));
    Scenes::Clear();
    Detail::ApplySceneChanges();
    CHECK(Calls == std::vector<std::string>({"A.Enter", "A.Exit", "Title.Exit"}));
    CHECK(Scenes::Current() == nullptr);
    Clean();
}

TEST(ScenesChangesFromUpdateWaitForTheEnd) {
    Clean();
    struct Switcher : Named {
        Switcher() : Named("Switcher") {}
        void Update() override {
            Named::Update();
            Scenes::Replace(MakeScene<Named>("Next"));
            Calls.push_back(Scenes::Current() == this ? "still me" : "gone");
        }
    };
    Scenes::Push(MakeScene<Switcher>());
    Detail::ApplySceneChanges();
    Calls.clear();
    Tick();
    CHECK(Calls == std::vector<std::string>({"Switcher.Update", "still me", "Switcher.Exit", "Next.Enter"}));
    Clean();
}

TEST(ScenesManagerAddedLaterStartsAtOnce) {
    Clean();
    struct Late : Named {
        Late() : Named("Late") { Add<Physics>(); }
        void Enter() override {
            Named::Enter();
            Add<Enemies>(); // running already: starts now
        }
    };
    Scenes::Push(MakeScene<Late>());
    Detail::ApplySceneChanges();
    CHECK(Calls == std::vector<std::string>({"Physics.Start", "Late.Enter", "Enemies.Start"}));
    CHECK(Scenes::Current()->Find<Enemies>() != nullptr);
    CHECK(Scenes::Current()->Find<Unused>() == nullptr);
    Clean();
}

TEST(ScenesTasksPauseWithTheSceneAndEndWithIt) {
    Clean();
    Scene& game = Scenes::Push(MakeScene<Named>("Game"));
    Detail::ApplySceneChanges();
    int beats = 0;
    Task task = game.Run(Sequence().Wait(0.5f).Do([&] { ++beats; }).Repeat());
    Ticks(30);
    CHECK_EQ(beats, 1);
    Scenes::Push(MakeScene<Named>("Pause"));
    Ticks(60); // covered: the game's tasks wait
    CHECK_EQ(beats, 1);
    Scenes::Pop();
    Ticks(1);
    Ticks(30);
    CHECK_EQ(beats, 2);
    Scenes::Pop();
    Ticks(1);
    CHECK(task.Done()); // cancelled when the scene exited
    Clean();
}

TEST(ScenesFadeChangesInTheMiddle) {
    Clean();
    Scenes::Push(MakeScene<Named>("Title"));
    Detail::ApplySceneChanges();
    Calls.clear();
    Scenes::Replace(MakeScene<Named>("Level"), Transition::Fade(1.0f));
    Tick(); // Title updates, then the fade starts
    CHECK(Scenes::InTransition());
    Calls.clear();
    Ticks(15);
    CHECK(Calls.empty()); // nothing updates while it goes dark
    CHECK_NEAR(Detail::SceneFade(), 0.5, 1e-3);
    Ticks(15);
    CHECK(Calls == std::vector<std::string>({"Title.Exit", "Level.Enter"}));
    CHECK_NEAR(Detail::SceneFade(), 1.0, 1e-3);
    Calls.clear();
    Ticks(15);
    CHECK_EQ(Calls.size(), std::size_t(15)); // the new scene updates while it comes back
    CHECK_NEAR(Detail::SceneFade(), 0.5, 1e-3);
    Ticks(15);
    CHECK(!Scenes::InTransition());
    CHECK_NEAR(Detail::SceneFade(), 0.0, 1e-6);
    Clean();
}

TEST(ScenesChangedEvent) {
    Clean();
    std::vector<Scene*> seen;
    Subscription on = Events::On<SceneChanged>([&](const SceneChanged& e) { seen.push_back(e.Current); });
    Scene& a = Scenes::Push(MakeScene<Named>("A"));
    Scene& b = Scenes::Push(MakeScene<Named>("B"));
    Detail::ApplySceneChanges(); // one event for the whole batch
    CHECK(seen == std::vector<Scene*>({&b}));
    Scenes::Pop();
    Detail::ApplySceneChanges();
    CHECK(seen == std::vector<Scene*>({&b, &a}));
    Clean();
}
