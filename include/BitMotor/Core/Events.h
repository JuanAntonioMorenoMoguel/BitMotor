#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// Events decouple parts of the game: whoever emits does not know who listens. Any struct is an event.
//     struct EnemyDied { Vec2 Position; int Score; };
//
//     Subscription onDeath = Events::On<EnemyDied>([this](const EnemyDied& e) { score += e.Score; });
//     Events::Emit(EnemyDied{position, 100});   // now: every listener runs before Emit returns
//     Events::Queue(EnemyDied{position, 100});  // at the end of the frame
//
// The Subscription stops listening when it is destroyed, so keep it as a member of whoever listens.
// Listeners may subscribe or cancel while an event is being delivered; new listeners hear the next one.
// Events are for the main thread only.
namespace BitMotor {

namespace Detail {
class ChannelBase {
public:
    virtual void Remove(std::uint64_t id) = 0;

protected:
    ~ChannelBase() = default;
};
std::uint64_t NextSubscriptionId();
void QueueEvent(std::function<void()> deliver);
} // namespace Detail

// Keeps a listener alive. Move-only; Cancel() or destroying it stops the listener.
class [[nodiscard]] Subscription {
public:
    Subscription() = default;
    Subscription(Detail::ChannelBase* channel, std::uint64_t id) : Channel(channel), Id(id) {}
    Subscription(Subscription&& other) noexcept
        : Channel(std::exchange(other.Channel, nullptr)), Id(std::exchange(other.Id, 0)) {}
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            Cancel();
            Channel = std::exchange(other.Channel, nullptr);
            Id = std::exchange(other.Id, 0);
        }
        return *this;
    }
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    ~Subscription() { Cancel(); }

    void Cancel() {
        if (Channel) Channel->Remove(Id);
        Channel = nullptr;
    }
    bool Active() const { return Channel != nullptr; }

private:
    Detail::ChannelBase* Channel = nullptr;
    std::uint64_t Id = 0;
};

namespace Detail {

template <class T> class Channel final : public ChannelBase {
public:
    // Never destroyed, so a Subscription that outlives everything (a global) can still cancel safely.
    static Channel& Get() {
        static Channel* channel = new Channel;
        return *channel;
    }

    std::uint64_t Add(std::function<void(const T&)> handler) {
        auto listener = std::make_shared<Listener>();
        listener->Id = NextSubscriptionId();
        listener->Handler = std::move(handler);
        Listeners.push_back(listener);
        return listener->Id;
    }

    void Remove(std::uint64_t id) override {
        for (std::size_t i = 0; i < Listeners.size(); ++i)
            if (Listeners[i]->Id == id) {
                Listeners[i]->Removed = true;
                Listeners.erase(Listeners.begin() + std::ptrdiff_t(i));
                return;
            }
    }

    void Emit(const T& event) {
        // A copy, so listeners can subscribe or cancel while the event is delivered.
        auto snapshot = Listeners;
        for (auto& listener : snapshot)
            if (!listener->Removed) listener->Handler(event);
    }

    std::size_t Count() const { return Listeners.size(); }

private:
    struct Listener {
        std::uint64_t Id = 0;
        std::function<void(const T&)> Handler;
        bool Removed = false;
    };
    std::vector<std::shared_ptr<Listener>> Listeners;
};

} // namespace Detail

namespace Events {

template <class T> Subscription On(std::function<void(const T&)> handler) {
    auto& channel = Detail::Channel<T>::Get();
    return {&channel, channel.Add(std::move(handler))};
}

// Delivers the event to every listener right now.
template <class T> void Emit(const T& event) { Detail::Channel<T>::Get().Emit(event); }

// Delivers the event at the end of the current frame. Events queued while the queue is being delivered
// wait for the next frame.
template <class T> void Queue(T event) {
    Detail::QueueEvent([event = std::move(event)] { Detail::Channel<T>::Get().Emit(event); });
}

// How many listeners an event type has (debug panels, tests).
template <class T> std::size_t Listeners() { return Detail::Channel<T>::Get().Count(); }

} // namespace Events

} // namespace BitMotor
