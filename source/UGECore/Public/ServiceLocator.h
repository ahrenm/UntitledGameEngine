#pragma once
#include <typeindex>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <format>

// ── ServiceLocator ────────────────────────────────────────────────────────────
// Lightweight type-safe service registry.
// Services are registered by pointer and looked up by type.
// Lifetime is managed externally — the locator does NOT own anything.
//
// Usage:
//   ServiceLocator::Provide(&myPhysFS);
//   auto& fs = ServiceLocator::Get<PhysFSLayer>();
//   ServiceLocator::Clear();  // on shutdown
//
class ServiceLocator
{
public:
    // Register a service. Overwrites any previous registration of the same type.
    template<typename T>
    static void Provide(T* Service)
    {
        registry()[std::type_index(typeid(T))] = static_cast<void*>(Service);
    }

    // Retrieve a service. Throws std::runtime_error if not registered.
    template<typename T>
    [[nodiscard]] static T& Get()
    {
        auto& Reg = registry();
        auto  It  = Reg.find(std::type_index(typeid(T)));
        if (It == Reg.end())
            throw std::runtime_error(
                std::format("ServiceLocator: '{}' has not been provided.", typeid(T).name()));
        return *static_cast<T*>(It->second);
    }

    // Returns nullptr if not registered instead of throwing.
    template<typename T>
    [[nodiscard]] static T* TryGet()
    {
        auto& Reg = registry();
        auto  It  = Reg.find(std::type_index(typeid(T)));
        return (It != Reg.end()) ? static_cast<T*>(It->second) : nullptr;
    }

    // Returns true if the service is registered.
    template<typename T>
    [[nodiscard]] static bool Has()
    {
        return registry().count(std::type_index(typeid(T))) > 0;
    }

    // Remove a specific service registration.
    template<typename T>
    static void Remove()
    {
        registry().erase(std::type_index(typeid(T)));
    }

    // Clear all registrations (call on shutdown before services are destroyed).
    static void Clear()
    {
        registry().clear();
    }

private:
    // Defined in ServiceLocator.cpp (compiled into UGECore.dll only).
    // All DLLs that link UGECore resolve this call through libUGECore.dll.a,
    // ensuring every caller shares the same map instance.
    static std::unordered_map<std::type_index, void*>& registry();
};
