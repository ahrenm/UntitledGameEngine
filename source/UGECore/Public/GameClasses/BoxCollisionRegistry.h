#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Forward declaration — BoxCollision.h includes us, so we must not include it.
struct BoxCollision;

class BoxCollisionRegistry;

// ── CollisionHandle ───────────────────────────────────────────────────────────
// RAII handle for explicit opt-in registration (when the box is not auto-registered).
// Deregisters on destruction.  Non-copyable, moveable.
class CollisionHandle
{
public:
    CollisionHandle() = default;
    ~CollisionHandle();

    CollisionHandle(const CollisionHandle&)            = delete;
    CollisionHandle& operator=(const CollisionHandle&) = delete;

    CollisionHandle(CollisionHandle&& Other) noexcept;
    CollisionHandle& operator=(CollisionHandle&& Other) noexcept;

    void Release();

    [[nodiscard]] bool     IsActive() const { return m_id != 0 && m_registry; }
    [[nodiscard]] uint32_t Id()       const { return m_id; }

private:
    friend class BoxCollisionRegistry;
    CollisionHandle(BoxCollisionRegistry* Registry, uint32_t Id);

    BoxCollisionRegistry* m_registry = nullptr;
    uint32_t              m_id       = 0;
};

// ── BoxCollisionRegistry ──────────────────────────────────────────────────────
// Non-owning registry of BoxCollision pointers for the debug overlay.
// Stored in SDLLayer.  Set as active via SetActive() so BoxCollision's
// value constructor can auto-register without a direct SDLLayer dependency.
class BoxCollisionRegistry
{
public:
    // ── Active singleton ───────────────────────────────────────────────────────
    // SDLLayer calls SetActive(&m_collisionRegistry) after construction and
    // SetActive(nullptr) before destruction.
    static void                        SetActive(BoxCollisionRegistry* R);
    [[nodiscard]] static BoxCollisionRegistry* Active();

    // ── Explicit opt-in (returns RAII handle) ──────────────────────────────────
    [[nodiscard]] CollisionHandle Register(BoxCollision* Box, std::string Label = "");

    // ── Auto-registration path (called by BoxCollision value constructor) ──────
    // Returns the new ID, or 0 if Box is already registered.
    uint32_t RegisterDirect(BoxCollision* Box, std::string_view Label);

    // Update the stored pointer when a BoxCollision is move-constructed/assigned.
    void UpdatePointer(uint32_t Id, BoxCollision* NewPtr);

    // Remove an entry by ID (called by CollisionHandle and BoxCollision destructor).
    void Deregister(uint32_t Id);

    struct Entry
    {
        uint32_t      id;
        BoxCollision* box;   // non-owning; guaranteed live while registered
        std::string   label;
    };

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_entries; }
    [[nodiscard]] bool Empty() const { return m_entries.empty(); }

private:
    std::vector<Entry>    m_entries;
    uint32_t              m_nextId  = 1;

    // Defined in BoxCollisionRegistry.cpp (UGECore.dll) — one definition across all DLLs.
    static BoxCollisionRegistry* s_active;
};
