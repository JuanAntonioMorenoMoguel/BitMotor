#include "Testing.h"

#include <BitMotor/BitMotor.h>

#include <string>
#include <vector>

using namespace BitMotor;

namespace {

constexpr float Step = 1.0f / 60.0f;

// Advances `list` `ticks` times at 60 Hz.
void Ticks(Detail::TaskList& list, int ticks) {
    for (int i = 0; i < ticks; ++i) list.Update(Step);
}

} // namespace

TEST(TasksWaitThenDoAtTheExactTick) {
    Detail::TaskList list;
    int fired = 0;
    Task task = list.Run(Sequence().Wait(1.0f).Do([&] { ++fired; }));
    Ticks(list, 59);
    CHECK_EQ(fired, 0);
    CHECK(!task.Done());
    Ticks(list, 1); // 60 fixed steps are one second, with no rounding drift
    CHECK_EQ(fired, 1);
    CHECK(task.Done());
    CHECK_EQ(list.Count(), std::size_t(0));
}

TEST(TasksLeftoverTimePassesToTheNextStep) {
    Detail::TaskList list;
    int fired = 0;
    list.Run(Sequence().Wait(0.25f).Wait(0.25f).Wait(0.5f).Do([&] { ++fired; }));
    Ticks(list, 59);
    CHECK_EQ(fired, 0);
    Ticks(list, 1);
    CHECK_EQ(fired, 1);

    // A long step (one Update of half a second) goes through several short waits at once.
    std::vector<int> order;
    list.Run(Sequence().Wait(0.1f).Do([&] { order.push_back(1); }).Wait(0.1f).Do([&] { order.push_back(2); }));
    list.Update(0.5f);
    CHECK(order == std::vector<int>({1, 2}));
}

TEST(TasksDoStepsRunInTheSameUpdate) {
    Detail::TaskList list;
    std::vector<int> order;
    list.Run(Sequence().Do([&] { order.push_back(1); }).Do([&] { order.push_back(2); }).Wait(Step).Do([&] {
        order.push_back(3);
    }));
    list.Update(0); // zero time: only the steps that take none
    CHECK(order == std::vector<int>({1, 2}));
    list.Update(Step);
    CHECK(order == std::vector<int>({1, 2, 3}));
}

TEST(TasksWaitUntil) {
    Detail::TaskList list;
    bool ready = false;
    int fired = 0;
    list.Run(Sequence().WaitUntil([&] { return ready; }).Do([&] { ++fired; }));
    Ticks(list, 10);
    CHECK_EQ(fired, 0);
    ready = true;
    Ticks(list, 1);
    CHECK_EQ(fired, 1);
}

TEST(TasksTweenFromCurrentValue) {
    Detail::TaskList list;
    float x = 10;
    Vec2 position{0, 0};
    list.Run(Sequence().Tween(x, 20, 1.0f).Tween(position, Vec2{6, 3}, 0.5f, Ease::Linear));
    x = 0; // changed before the task starts: the tween goes from here
    Ticks(list, 30);
    CHECK_NEAR(x, 10, 1e-4);
    Ticks(list, 30);
    CHECK_NEAR(x, 20, 1e-6); // lands exactly on the target
    Ticks(list, 15);
    CHECK_NEAR(position.X, 3, 1e-4);
    CHECK_NEAR(position.Y, 1.5, 1e-4);
    Ticks(list, 15);
    CHECK_NEAR(position.X, 6, 1e-6);
    CHECK_EQ(list.Count(), std::size_t(0));
}

TEST(TasksParallelWaitsForAll) {
    Detail::TaskList list;
    std::vector<std::string> order;
    list.Run(Sequence()
                 .Parallel(Sequence().Wait(0.5f).Do([&] { order.push_back("short"); }),
                           Sequence().Wait(1.0f).Do([&] { order.push_back("long"); }))
                 .Do([&] { order.push_back("after"); }));
    Ticks(list, 30);
    CHECK(order == std::vector<std::string>({"short"}));
    Ticks(list, 30);
    CHECK(order == std::vector<std::string>({"short", "long", "after"}));
}

TEST(TasksRepeat) {
    Detail::TaskList list;
    int beats = 0, done = 0;
    list.Run(Sequence().Wait(0.5f).Do([&] { ++beats; }).Repeat(3).Do([&] { ++done; }));
    Ticks(list, 89);
    CHECK_EQ(beats, 2);
    Ticks(list, 1);
    CHECK_EQ(beats, 3);
    CHECK_EQ(done, 1);
    Ticks(list, 60);
    CHECK_EQ(beats, 3); // finished
}

TEST(TasksRepeatForeverUntilCancelled) {
    Detail::TaskList list;
    int beats = 0, instant = 0;
    Task task = list.Run(Sequence().Wait(0.25f).Do([&] { ++beats; }).Repeat());
    // A loop with no waits goes round once per Update, never forever inside one.
    Task spin = list.Run(Sequence().Do([&] { ++instant; }).Repeat());
    Ticks(list, 60);
    CHECK_EQ(beats, 4);
    CHECK_EQ(instant, 60);
    task.Cancel();
    spin.Cancel();
    Ticks(list, 60);
    CHECK_EQ(beats, 4);
    CHECK(task.Done());
    CHECK_EQ(list.Count(), std::size_t(0));
}

TEST(TasksCancelFromItsOwnStep) {
    Detail::TaskList list;
    Task self;
    int after = 0;
    self = list.Run(Sequence().Do([&] { self.Cancel(); }).Do([&] { ++after; }));
    Ticks(list, 1);
    CHECK_EQ(after, 0);
    CHECK(self.Done());
}

TEST(TasksSequenceCanRunTwiceWithItsOwnState) {
    Detail::TaskList list;
    int count = 0;
    Sequence blink = Sequence().Wait(0.5f).Do([&] { ++count; });
    list.Run(blink);
    Ticks(list, 15);
    list.Run(blink); // starts from the beginning, not from where the first one is
    Ticks(list, 15);
    CHECK_EQ(count, 1);
    Ticks(list, 15);
    CHECK_EQ(count, 2);
}

TEST(TasksStartedDuringUpdateBeginNextTime) {
    Detail::TaskList list;
    int inner = 0;
    list.Run(Sequence().Do([&] { list.Run(Sequence().Do([&] { ++inner; })); }));
    list.Update(Step);
    CHECK_EQ(inner, 0);
    list.Update(Step);
    CHECK_EQ(inner, 1);
}

TEST(TasksClearCancelsAll) {
    Detail::TaskList list;
    Task a = list.Run(Sequence().Wait(1));
    Task b = list.Run(Sequence().Wait(2));
    CHECK_EQ(list.Count(), std::size_t(2));
    list.Clear();
    CHECK(a.Done());
    CHECK(b.Done());
    CHECK_EQ(list.Count(), std::size_t(0));
    CHECK(Task().Done()); // a handle that was never started
}

TEST(TasksGlobalRun) {
    Tasks::CancelAll();
    Task task = Tasks::Run(Sequence().Wait(1));
    CHECK_EQ(Tasks::Count(), std::size_t(1));
    Tasks::CancelAll();
    CHECK(task.Done());
    CHECK_EQ(Tasks::Count(), std::size_t(0));
}
