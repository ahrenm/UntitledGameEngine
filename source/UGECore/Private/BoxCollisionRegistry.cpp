#include <GameClasses/BoxCollisionRegistry.h>
#include <algorithm>

// Single definition of s_active — all DLLs resolve to this one symbol in UGECore.dll.
BoxCollisionRegistry* BoxCollisionRegistry::s_active = nullptr;

void BoxCollisionRegistry::SetActive(BoxCollisionRegistry* R) { s_active = R; }
BoxCollisionRegistry* BoxCollisionRegistry::Active()          { return s_active; }

// ── CollisionHandle ───────────────────────────────────────────────────────────

CollisionHandle::CollisionHandle(BoxCollisionRegistry* Registry, uint32_t Id)
    : m_registry(Registry), m_id(Id) {}

CollisionHandle::~CollisionHandle() { Release(); }

CollisionHandle::CollisionHandle(CollisionHandle&& Other) noexcept
    : m_registry(Other.m_registry), m_id(Other.m_id)
{
    Other.m_registry = nullptr;
    Other.m_id       = 0;
}

CollisionHandle& CollisionHandle::operator=(CollisionHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        m_registry       = Other.m_registry;
        m_id             = Other.m_id;
        Other.m_registry = nullptr;
        Other.m_id       = 0;
    }
    return *this;
}

void CollisionHandle::Release()
{
    if (m_id != 0 && m_registry)
        m_registry->Deregister(m_id);
    m_id       = 0;
    m_registry = nullptr;
}

// ── BoxCollisionRegistry ──────────────────────────────────────────────────────

CollisionHandle BoxCollisionRegistry::Register(BoxCollision* Box, std::string Label)
{
    const uint32_t Id = m_nextId++;
    m_entries.push_back({ Id, Box, std::move(Label) });
    return CollisionHandle(this, Id);
}

uint32_t BoxCollisionRegistry::RegisterDirect(BoxCollision* Box, std::string_view Label)
{
    // Guard against double-registration (e.g. if somehow called twice).
    for (const auto& E : m_entries)
        if (E.box == Box) return E.id;

    const uint32_t Id = m_nextId++;
    m_entries.push_back({ Id, Box, std::string(Label) });
    return Id;
}

void BoxCollisionRegistry::UpdatePointer(uint32_t Id, BoxCollision* NewPtr)
{
    for (auto& E : m_entries)
        if (E.id == Id) { E.box = NewPtr; return; }
}

void BoxCollisionRegistry::Deregister(uint32_t Id)
{
    std::erase_if(m_entries, [Id](const Entry& E) { return E.id == Id; });
}


