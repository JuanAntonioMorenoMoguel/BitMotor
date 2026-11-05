#include "BitMotor/Core/Events.h"

#include "Core/EventsInternal.h"

namespace BitMotor::Detail {

namespace {
std::uint64_t LastId = 0;
std::vector<std::function<void()>> Pending;
} // namespace

std::uint64_t NextSubscriptionId() { return ++LastId; }

void QueueEvent(std::function<void()> deliver) { Pending.push_back(std::move(deliver)); }

void FlushEvents() {
    std::vector<std::function<void()>> now;
    now.swap(Pending);
    for (auto& deliver : now) deliver();
}

} // namespace BitMotor::Detail
