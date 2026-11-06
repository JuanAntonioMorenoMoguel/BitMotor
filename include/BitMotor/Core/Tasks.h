#pragma once
#include "BitMotor/Math/Ease.h"
#include "BitMotor/Math/Functions.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// Tasks: things that happen over time, written as one sequence instead of timers and phase counters.
//     Task intro = Tasks::Run(Sequence()
//         .Wait(0.5f)
//         .Tween(titleY, 40.0f, 0.6f, Ease::OutBack)    // from its current value, to 40
//         .WaitUntil([] { return Input::Pressed(Action::Confirm); })
//         .Parallel(Sequence().Tween(alpha, 0.0f, 0.3f),
//                   Sequence().Wait(0.1f).Do([] { Log::Info("click"); }))
//         .Do([] { Log::Info("go!"); }));
//     intro.Done();     // finished or cancelled
//     intro.Cancel();   // stops it where it is
//
// Tasks advance once per Update with Time::Delta, so they follow the TickRate, Time::SetScale and
// Time::HitStop like the rest of the game. Steps that take no time (Do, a WaitUntil already true) run in
// the same Update as the step before them, and time left over from a Wait passes on to the next step,
// so Wait(0.5f).Wait(0.5f) lasts exactly one second.
//
// A Sequence is only a description: it can be kept and run several times, each run with its own state.
// Destroying a Task handle does not stop the task. Values a Tween changes are kept by reference: they
// must outlive the task (a Scene's tasks, from phase 2.2, end with the scene).
namespace BitMotor {

namespace Detail {

// One step of a sequence. Advance consumes time from `dt` and returns true when the step is finished;
// whatever is left in `dt` goes to the next step.
class Step {
public:
    virtual ~Step() = default;
    virtual bool Advance(float& dt) = 0;
    // Back to the beginning (Repeat).
    virtual void Reset() = 0;
    // A copy at the beginning, for a new run.
    virtual std::unique_ptr<Step> Clone() const = 0;
};

// Wait and Tween are done when they reach their time, give or take this much rounding.
inline constexpr float StepEpsilon = 1e-5f;

template <class T> class TweenStep final : public Step {
public:
    TweenStep(T* value, T to, float duration, Ease::Function easing)
        : Value(value), To(to), Duration(duration), Easing(easing) {}

    bool Advance(float& dt) override {
        if (!Started) {
            From = *Value;
            Started = true;
        }
        float step = Min(dt, Max(Duration - Elapsed, 0.0f));
        Elapsed += step;
        dt -= step;
        bool done = Elapsed >= Duration - StepEpsilon;
        float t = done || Duration <= 0 ? 1.0f : Elapsed / Duration;
        *Value = Lerp(From, To, Easing ? Easing(t) : t);
        return done;
    }
    void Reset() override {
        Elapsed = 0;
        Started = false;
    }
    std::unique_ptr<Step> Clone() const override { return std::make_unique<TweenStep>(Value, To, Duration, Easing); }

private:
    T* Value;
    T From{}, To;
    float Duration;
    Ease::Function Easing;
    float Elapsed = 0;
    bool Started = false;
};

struct TaskState;

} // namespace Detail

// A list of steps that run one after another. Build it by chaining:
//     Sequence().Wait(1).Do(...).Repeat(3)
class Sequence {
public:
    Sequence();
    ~Sequence();
    Sequence(const Sequence& other);
    Sequence& operator=(const Sequence& other);
    Sequence(Sequence&& other) noexcept;
    Sequence& operator=(Sequence&& other) noexcept;

    // Waits `seconds` of game time.
    Sequence& Wait(float seconds);
    // Waits until `condition` returns true (checked once per Update, the first time right away).
    Sequence& WaitUntil(std::function<bool()> condition);
    // Runs `action` once.
    Sequence& Do(std::function<void()> action);
    // Takes `value` from what it is when the step starts to `to` in `seconds`, following `easing`.
    // Works with float, Vec2, Vec3, Vec4, Color and any type with a Lerp(a, b, t).
    template <class T, class U> Sequence& Tween(T& value, U to, float seconds, Ease::Function easing = Ease::Linear) {
        return Add(std::make_unique<Detail::TweenStep<T>>(&value, T(to), seconds, easing));
    }
    // Runs several sequences at the same time and goes on when all of them have finished.
    template <class... S> Sequence& Parallel(S&&... sequences) {
        std::vector<Sequence> list;
        (list.push_back(Sequence(std::forward<S>(sequences))), ...);
        return AddParallel(std::move(list));
    }
    // Repeats every step written before it `times` times in total; 0: forever (until cancelled).
    // A forever loop whose steps take no time waits for the next Update before going round again.
    Sequence& Repeat(int times = 0);

    // Number of steps (a Parallel or a Repeat counts as one).
    std::size_t Size() const { return Steps.size(); }

    // Internal: advances the whole sequence (Detail::Step::Advance).
    bool Advance(float& dt);
    void Reset();

private:
    Sequence& Add(std::unique_ptr<Detail::Step> step);
    Sequence& AddParallel(std::vector<Sequence> sequences);

    std::vector<std::unique_ptr<Detail::Step>> Steps;
    std::size_t Current = 0;
};

// A running sequence. Copies refer to the same task; destroying them does not stop it.
class Task {
public:
    Task() = default;
    explicit Task(std::shared_ptr<Detail::TaskState> state) : State(std::move(state)) {}

    // Stops the task where it is: none of its remaining steps run.
    void Cancel();
    // Finished or cancelled (also true for a Task that was never started).
    bool Done() const;

private:
    std::shared_ptr<Detail::TaskState> State;
};

namespace Detail {

struct TaskState {
    Sequence Steps;
    bool Finished = false;
};

// Tasks that advance together: the global ones (Tasks::Run) and, from phase 2.2, each Scene's.
class TaskList {
public:
    Task Run(Sequence sequence);
    // Advances every task by `dt`. Tasks started during it begin on the next call.
    void Update(float dt);
    // Cancels them all.
    void Clear();
    std::size_t Count() const;

private:
    std::vector<std::shared_ptr<TaskState>> Running;
};

} // namespace Detail

namespace Tasks {

// Starts a task that belongs to the whole game: it advances every Update, after the Services and before
// Game::Update, and is cancelled when the game ends.
Task Run(Sequence sequence);
// Cancels every global task.
void CancelAll();
// Global tasks still running (debug panels, tests).
std::size_t Count();

} // namespace Tasks

} // namespace BitMotor
