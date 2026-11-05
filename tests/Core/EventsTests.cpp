#include "Testing.h"

#include "Core/EventsInternal.h"

#include <BitMotor/BitMotor.h>

#include <vector>

using namespace BitMotor;

namespace {
struct Hit {
    int Damage = 0;
};
struct Other {};
} // namespace

TEST(EventsEmitReachesEveryListenerInOrder) {
    std::vector<int> seen;
    Subscription a = Events::On<Hit>([&](const Hit& e) { seen.push_back(e.Damage); });
    Subscription b = Events::On<Hit>([&](const Hit& e) { seen.push_back(e.Damage * 10); });
    Events::Emit(Hit{3});
    Events::Emit(Other{}); // other types do not reach Hit listeners
    CHECK(seen == std::vector<int>({3, 30}));
    CHECK_EQ(Events::Listeners<Hit>(), std::size_t(2));
}

TEST(EventsSubscriptionStopsWhenDestroyedOrCancelled) {
    int count = 0;
    {
        Subscription s = Events::On<Hit>([&](const Hit&) { ++count; });
        Events::Emit(Hit{});
    }
    Events::Emit(Hit{});
    CHECK_EQ(count, 1);
    Subscription s = Events::On<Hit>([&](const Hit&) { ++count; });
    Subscription moved = std::move(s);
    CHECK(!s.Active());
    CHECK(moved.Active());
    Events::Emit(Hit{});
    moved.Cancel();
    Events::Emit(Hit{});
    CHECK_EQ(count, 2);
    CHECK_EQ(Events::Listeners<Hit>(), std::size_t(0));
}

TEST(EventsListenersCanCancelAndSubscribeWhileDelivering) {
    int first = 0, second = 0, late = 0;
    Subscription b;
    Subscription added;
    Subscription a = Events::On<Hit>([&](const Hit&) {
        ++first;
        b.Cancel(); // b must not run in this same delivery
        if (!added.Active()) added = Events::On<Hit>([&](const Hit&) { ++late; });
    });
    b = Events::On<Hit>([&](const Hit&) { ++second; });
    Events::Emit(Hit{});
    CHECK_EQ(first, 1);
    CHECK_EQ(second, 0);
    CHECK_EQ(late, 0); // added during the delivery: hears the next one
    Events::Emit(Hit{});
    CHECK_EQ(late, 1);
}

TEST(EventsQueueWaitsForTheEndOfTheFrame) {
    std::vector<int> seen;
    Subscription s = Events::On<Hit>([&](const Hit& e) {
        seen.push_back(e.Damage);
        if (e.Damage == 1) Events::Queue(Hit{2}); // queued while delivering: next frame
    });
    Events::Queue(Hit{1});
    CHECK(seen.empty());
    Detail::FlushEvents();
    CHECK(seen == std::vector<int>({1}));
    Detail::FlushEvents();
    CHECK(seen == std::vector<int>({1, 2}));
}
