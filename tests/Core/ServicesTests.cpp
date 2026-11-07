#include "Testing.h"

#include "Core/ServicesInternal.h"

#include <BitMotor/BitMotor.h>

#include <string>
#include <vector>

using namespace BitMotor;

namespace {

std::vector<std::string> Calls;

struct Score : Service {
    int Points;
    explicit Score(int points = 0) : Points(points) {}
    void Start() override { Calls.push_back("Score.Start"); }
    void Update() override { Calls.push_back("Score.Update"); }
    void Stop() override { Calls.push_back("Score.Stop"); }
};

struct Jukebox : Service {
    void Start() override { Calls.push_back("Jukebox.Start"); }
    void Update() override { Calls.push_back("Jukebox.Update"); }
    void Stop() override { Calls.push_back("Jukebox.Stop"); }
};

struct SaveBackend : Service {
    virtual std::string Where() const = 0;
};
struct LocalSave : SaveBackend {
    std::string Where() const override { return "local"; }
};

// Removes itself while the services update.
struct OneShot : Service {
    void Update() override {
        Calls.push_back("OneShot.Update");
        Services::Remove<OneShot>();
    }
    void Stop() override { Calls.push_back("OneShot.Stop"); }
};

void Clean() {
    Detail::StopServices();
    Calls.clear();
}

} // namespace

TEST(ServicesAddStartsAndGetFinds) {
    Clean();
    Score& score = Services::Add<Score>(5);
    CHECK_EQ(score.Points, 5);
    CHECK(Calls == std::vector<std::string>({"Score.Start"}));
    CHECK_EQ(&Services::Get<Score>(), &score);
    CHECK_EQ(Services::Find<Score>(), &score);
    CHECK(Services::Has<Score>());
    CHECK(!Services::Has<Jukebox>());
    CHECK(Services::Find<Jukebox>() == nullptr);
    Clean();
}

TEST(ServicesUpdateInOrderAndStopInReverse) {
    Clean();
    Services::Add<Score>();
    Services::Add<Jukebox>();
    Calls.clear();
    Detail::UpdateServices();
    CHECK(Calls == std::vector<std::string>({"Score.Update", "Jukebox.Update"}));
    Calls.clear();
    Detail::StopServices();
    CHECK(Calls == std::vector<std::string>({"Jukebox.Stop", "Score.Stop"}));
    CHECK_EQ(Detail::ServiceCount(), std::size_t(0));
    CHECK(!Services::Has<Score>());
}

TEST(ServicesRemoveStopsIt) {
    Clean();
    Services::Add<Score>();
    Services::Add<Jukebox>();
    Calls.clear();
    CHECK(Services::Remove<Score>());
    CHECK(Calls == std::vector<std::string>({"Score.Stop"}));
    CHECK(!Services::Remove<Score>()); // already gone
    CHECK(!Services::Has<Score>());
    Services::Add<Score>(); // can come back
    CHECK(Services::Has<Score>());
    Clean();
}

TEST(ServicesByInterface) {
    Clean();
    LocalSave& local = Services::Add<SaveBackend, LocalSave>();
    CHECK_EQ(Services::Get<SaveBackend>().Where(), std::string("local"));
    CHECK_EQ(Services::Find<SaveBackend>(), static_cast<SaveBackend*>(&local));
    CHECK(!Services::Has<LocalSave>()); // asked for by the interface only
    Clean();
}

TEST(ServicesRemovedWhileUpdatingDoesNotBreakTheLoop) {
    Clean();
    Services::Add<OneShot>();
    Services::Add<Jukebox>();
    Calls.clear();
    Detail::UpdateServices();
    CHECK(Calls == std::vector<std::string>({"OneShot.Update", "OneShot.Stop", "Jukebox.Update"}));
    CHECK_EQ(Detail::ServiceCount(), std::size_t(1));
    Calls.clear();
    Detail::UpdateServices();
    CHECK(Calls == std::vector<std::string>({"Jukebox.Update"}));
    Clean();
}
