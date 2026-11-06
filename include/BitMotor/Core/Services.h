#pragma once
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

// Services: one object of each kind for the whole game (progress, save backend, music...), alive across
// scenes. Add them in Game::Start; they update before the game and the scenes, in the order they were added.
//     struct Progress : Service {
//         int Coins = 0;
//         void Start() override { Coins = Data::Load<int>("coins.json").value_or(0); }
//         void Stop() override { Data::Save("coins.json", Coins); }
//     };
//
//     Services::Add<Progress>();                 // calls Start; returns Progress&
//     Services::Get<Progress>().Coins += 10;     // anywhere; if it was never added: error and abort
//     if (Progress* p = Services::Find<Progress>()) ...   // nullptr if missing
//     Services::Add<SaveBackend, LocalSave>();   // asked for by the interface: Get<SaveBackend>()
//
// When the game ends they stop in reverse order, after the game's own Stop. Services are for the main
// thread only.
namespace BitMotor {

class Service {
public:
    virtual ~Service() = default;
    // Right after Services::Add.
    virtual void Start() {}
    // Once per Update (TickRate times per second), before Game::Update.
    virtual void Update() {}
    // On Services::Remove or when the game ends.
    virtual void Stop() {}
};

namespace Detail {
using TypeKey = const void*;
// A unique address per type, without needing RTTI to compare.
template <class T> TypeKey KeyOf() {
    static const char key = 0;
    return &key;
}
void AddService(TypeKey key, const char* name, std::unique_ptr<Service> service);
Service* FindService(TypeKey key);
bool RemoveService(TypeKey key);
[[noreturn]] void MissingService(const char* name);
} // namespace Detail

namespace Services {

// Creates the service with `args`, calls its Start and returns it. `T` is the type it is asked for by;
// `Impl` (T by default) the type actually created. Adding the same T twice is an error (abort).
template <class T, class Impl = T, class... A> Impl& Add(A&&... args) {
    static_assert(std::is_base_of_v<Service, T>, "Services::Add<T>: T must derive from BitMotor::Service");
    static_assert(std::is_base_of_v<T, Impl>, "Services::Add<T, Impl>: Impl must derive from T");
    auto service = std::make_unique<Impl>(std::forward<A>(args)...);
    Impl& result = *service;
    Detail::AddService(Detail::KeyOf<T>(), typeid(T).name(), std::move(service));
    return result;
}

// The service, or nullptr if it was not added.
template <class T> T* Find() { return static_cast<T*>(Detail::FindService(Detail::KeyOf<T>())); }

// The service. If it was not added the game cannot go on: logs an error and aborts.
template <class T> T& Get() {
    if (T* service = Find<T>()) return *service;
    Detail::MissingService(typeid(T).name());
}

template <class T> bool Has() { return Find<T>() != nullptr; }

// Calls its Stop and destroys it. false if it was not there.
template <class T> bool Remove() { return Detail::RemoveService(Detail::KeyOf<T>()); }

} // namespace Services

} // namespace BitMotor
