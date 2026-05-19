#pragma once
#include "../Layers/LoggingLayer.h"
#include "../Layers/PhysFSLayer.h"
#include "../Layers/UDEDataLayer.h"
#include "../Layers/LuaLayer.h"
#include "../Layers/SDLLayer.h"
#include "../Layers/RmlUILayer.h"
#include "../ServiceLocator.h"

// ── GameObjectBase ────────────────────────────────────────────────────────────
// Common base class for Scenes and ViewModels.
// Includes all engine layer headers and exposes protected accessors to each
// layer via the ServiceLocator.
//
// All accessors return raw non-owning pointers — nullptr if the layer has not
// yet been registered (e.g. during early startup).  Callers should guard with
// a null-check when the availability of a layer cannot be guaranteed.
//
// Example (inside a Scene or ViewModel method):
//   if (auto* Data = GetDataLayer())
//       auto gravity = Data->Data.GetFloat("platformerData", "physics.gravity");
class GameObjectBase
{
public:
    virtual ~GameObjectBase() = default;

protected:
    [[nodiscard]] static LoggingLayer*   GetLoggingLayer()   { return ServiceLocator::TryGet<LoggingLayer>(); }
    [[nodiscard]] static PhysFSLayer*    GetPhysFSLayer()    { return ServiceLocator::TryGet<PhysFSLayer>(); }
    [[nodiscard]] static UDEDataLayer*   GetDataLayer()      { return ServiceLocator::TryGet<UDEDataLayer>(); }
    [[nodiscard]] static LuaLayer*       GetLuaLayer()       { return ServiceLocator::TryGet<LuaLayer>(); }
    [[nodiscard]] static SDLLayer*       GetSDLLayer()       { return ServiceLocator::TryGet<SDLLayer>(); }
    [[nodiscard]] static RmlUILayer*     GetUILayer()        { return ServiceLocator::TryGet<RmlUILayer>(); }
};

