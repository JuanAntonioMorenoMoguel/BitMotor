#include "BitMotor/Core/Services.h"

#include "BitMotor/Core/Log.h"
#include "Core/ServicesInternal.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace BitMotor::Detail {

namespace {

struct Entry {
    TypeKey Key = nullptr;
    std::string Name;
    std::unique_ptr<Service> Object;
};

// In the order they were added. A service removed while the list is being updated is left empty and
// erased afterwards, so the loop never skips or repeats one.
std::vector<Entry> List;
int Updating = 0;

Entry* FindEntry(TypeKey key) {
    for (Entry& entry : List)
        if (entry.Key == key && entry.Object) return &entry;
    return nullptr;
}

void EraseEmpty() {
    std::erase_if(List, [](const Entry& entry) { return !entry.Object; });
}

} // namespace

void AddService(TypeKey key, const char* name, std::unique_ptr<Service> service) {
    if (FindEntry(key)) {
        Log::Error("Services::Add<{}>: it was already added", name);
        std::abort();
    }
    Service* object = service.get();
    List.push_back({key, name, std::move(service)});
    object->Start();
}

Service* FindService(TypeKey key) {
    Entry* entry = FindEntry(key);
    return entry ? entry->Object.get() : nullptr;
}

bool RemoveService(TypeKey key) {
    Entry* entry = FindEntry(key);
    if (!entry) return false;
    std::unique_ptr<Service> object = std::move(entry->Object);
    if (!Updating) EraseEmpty();
    object->Stop();
    return true;
}

void MissingService(const char* name) {
    Log::Error("Services::Get<{}>: it was never added (Services::Add in Game::Start)", name);
    std::abort();
}

void UpdateServices() {
    ++Updating;
    // By index: a service may add another one while it updates (it starts updating next time).
    const std::size_t count = List.size();
    for (std::size_t i = 0; i < count && i < List.size(); ++i)
        if (Service* object = List[i].Object.get()) object->Update();
    --Updating;
    if (!Updating) EraseEmpty();
}

void StopServices() {
    while (!List.empty()) {
        std::unique_ptr<Service> object = std::move(List.back().Object);
        List.pop_back();
        if (object) object->Stop();
    }
}

std::size_t ServiceCount() {
    std::size_t count = 0;
    for (const Entry& entry : List)
        if (entry.Object) ++count;
    return count;
}

} // namespace BitMotor::Detail
