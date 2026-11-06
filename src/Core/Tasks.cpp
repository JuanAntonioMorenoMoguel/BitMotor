#include "BitMotor/Core/Tasks.h"

#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Time.h"
#include "Core/TasksInternal.h"

#include <algorithm>

namespace BitMotor {

namespace {

// Set while a task advances: a task cancelled from one of its own steps stops right there.
const bool* CurrentFinished = nullptr;

bool CurrentCancelled() { return CurrentFinished && *CurrentFinished; }

class WaitStep final : public Detail::Step {
public:
    explicit WaitStep(float duration) : Duration(duration) {}
    bool Advance(float& dt) override {
        float step = Min(dt, Max(Duration - Elapsed, 0.0f));
        Elapsed += step;
        dt -= step;
        return Elapsed >= Duration - Detail::StepEpsilon;
    }
    void Reset() override { Elapsed = 0; }
    std::unique_ptr<Detail::Step> Clone() const override { return std::make_unique<WaitStep>(Duration); }

private:
    float Duration;
    float Elapsed = 0;
};

class WaitUntilStep final : public Detail::Step {
public:
    explicit WaitUntilStep(std::function<bool()> condition) : Condition(std::move(condition)) {}
    bool Advance(float& dt) override {
        if (!Condition || Condition()) return true;
        dt = 0; // the rest of this Update is spent waiting
        return false;
    }
    void Reset() override {}
    std::unique_ptr<Detail::Step> Clone() const override { return std::make_unique<WaitUntilStep>(Condition); }

private:
    std::function<bool()> Condition;
};

class DoStep final : public Detail::Step {
public:
    explicit DoStep(std::function<void()> action) : Action(std::move(action)) {}
    bool Advance(float&) override {
        if (Action) Action();
        return true;
    }
    void Reset() override {}
    std::unique_ptr<Detail::Step> Clone() const override { return std::make_unique<DoStep>(Action); }

private:
    std::function<void()> Action;
};

class ParallelStep final : public Detail::Step {
public:
    explicit ParallelStep(std::vector<Sequence> sequences)
        : Sequences(std::move(sequences)), Finished(Sequences.size(), false) {}
    bool Advance(float& dt) override {
        bool all = true;
        float leftover = dt;
        for (std::size_t i = 0; i < Sequences.size(); ++i) {
            if (Finished[i]) continue;
            float own = dt;
            if (Sequences[i].Advance(own))
                Finished[i] = true;
            else
                all = false;
            leftover = Min(leftover, own);
            if (CurrentCancelled()) return false;
        }
        dt = all ? leftover : 0.0f;
        return all;
    }
    void Reset() override {
        for (Sequence& sequence : Sequences) sequence.Reset();
        std::fill(Finished.begin(), Finished.end(), false);
    }
    std::unique_ptr<Detail::Step> Clone() const override { return std::make_unique<ParallelStep>(Sequences); }

private:
    std::vector<Sequence> Sequences;
    std::vector<bool> Finished;
};

class RepeatStep final : public Detail::Step {
public:
    RepeatStep(Sequence body, int times) : Body(std::move(body)), Times(times) {}
    bool Advance(float& dt) override {
        while (true) {
            const float before = dt;
            if (!Body.Advance(dt)) return false;
            ++Count;
            if (Times > 0 && Count >= Times) return true;
            Body.Reset();
            if (CurrentCancelled()) return false;
            // A forever loop that took no time would never end this Update: go round again next time.
            if (Times == 0 && dt == before) return false;
        }
    }
    void Reset() override {
        Body.Reset();
        Count = 0;
    }
    std::unique_ptr<Detail::Step> Clone() const override { return std::make_unique<RepeatStep>(Body, Times); }

private:
    Sequence Body;
    int Times;
    int Count = 0;
};

} // namespace

// ---------------------------------------------------------------------------------------------- Sequence

Sequence::Sequence() = default;
Sequence::~Sequence() = default;
Sequence::Sequence(Sequence&& other) noexcept = default;
Sequence& Sequence::operator=(Sequence&& other) noexcept = default;

Sequence::Sequence(const Sequence& other) {
    Steps.reserve(other.Steps.size());
    for (const auto& step : other.Steps) Steps.push_back(step->Clone());
}

Sequence& Sequence::operator=(const Sequence& other) {
    if (this != &other) *this = Sequence(other);
    return *this;
}

Sequence& Sequence::Add(std::unique_ptr<Detail::Step> step) {
    Steps.push_back(std::move(step));
    return *this;
}

Sequence& Sequence::Wait(float seconds) { return Add(std::make_unique<WaitStep>(seconds)); }

Sequence& Sequence::WaitUntil(std::function<bool()> condition) {
    return Add(std::make_unique<WaitUntilStep>(std::move(condition)));
}

Sequence& Sequence::Do(std::function<void()> action) { return Add(std::make_unique<DoStep>(std::move(action))); }

Sequence& Sequence::AddParallel(std::vector<Sequence> sequences) {
    for (Sequence& sequence : sequences) sequence.Reset();
    return Add(std::make_unique<ParallelStep>(std::move(sequences)));
}

Sequence& Sequence::Repeat(int times) {
    if (times < 0) {
        Log::Warn("Sequence::Repeat({}): needs 0 (forever) or more; repeating once", times);
        times = 1;
    }
    Sequence body;
    body.Steps = std::move(Steps);
    Steps.clear();
    Current = 0;
    return Add(std::make_unique<RepeatStep>(std::move(body), times));
}

bool Sequence::Advance(float& dt) {
    while (Current < Steps.size()) {
        if (!Steps[Current]->Advance(dt)) return false;
        ++Current;
        if (CurrentCancelled()) return false;
    }
    return true;
}

void Sequence::Reset() {
    for (auto& step : Steps) step->Reset();
    Current = 0;
}

// -------------------------------------------------------------------------------------------------- Task

void Task::Cancel() {
    if (State) State->Finished = true;
}

bool Task::Done() const { return !State || State->Finished; }

// ---------------------------------------------------------------------------------------------- TaskList

namespace Detail {

Task TaskList::Run(Sequence sequence) {
    auto state = std::make_shared<TaskState>();
    state->Steps = std::move(sequence);
    state->Steps.Reset();
    Running.push_back(state);
    return Task(state);
}

void TaskList::Update(float dt) {
    // A copy: a step may start or cancel tasks of this same list.
    auto snapshot = Running;
    for (auto& state : snapshot) {
        if (state->Finished) continue;
        const bool* outer = CurrentFinished;
        CurrentFinished = &state->Finished;
        float own = dt;
        if (state->Steps.Advance(own)) state->Finished = true;
        CurrentFinished = outer;
    }
    std::erase_if(Running, [](const std::shared_ptr<TaskState>& state) { return state->Finished; });
}

void TaskList::Clear() {
    auto stopped = std::move(Running);
    Running.clear();
    for (auto& state : stopped) state->Finished = true;
}

std::size_t TaskList::Count() const {
    return std::size_t(std::count_if(Running.begin(), Running.end(),
                                     [](const std::shared_ptr<TaskState>& state) { return !state->Finished; }));
}

namespace {
TaskList& Global() {
    static TaskList list;
    return list;
}
} // namespace

void UpdateTasks() { Global().Update(Time::Delta()); }
void ClearTasks() { Global().Clear(); }

} // namespace Detail

// ------------------------------------------------------------------------------------------------- Tasks

namespace Tasks {

Task Run(Sequence sequence) { return Detail::Global().Run(std::move(sequence)); }
void CancelAll() { Detail::ClearTasks(); }
std::size_t Count() { return Detail::Global().Count(); }

} // namespace Tasks

} // namespace BitMotor
