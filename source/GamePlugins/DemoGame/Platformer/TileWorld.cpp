#include "TileWorld.h"
#include <Layers/UGEDataLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>
#include <Sprite/Sprite.h>

// ── Build ─────────────────────────────────────────────────────────────────────
void TileWorld::Build(const char* DataKey, const char* MapKey)
{
    buildTileSprites(DataKey);
    buildTileMap(MapKey);
}

// ── buildTileSprites (private) ────────────────────────────────────────────────
// Loads each named tile sprite from paths provided by the DataLayer dataset.
// Missing entries are logged; the colour fallback path in Tick() is used
// automatically for any tile whose Spr remains nullptr.
void TileWorld::buildTileSprites(const char* DataKey)
{
    auto* dataLayer = ServiceLocator::TryGet<UGEDataLayer>();
    auto* logLayer  = ServiceLocator::TryGet<LoggingLayer>();

    if (!dataLayer) return;

    struct Entry { const char* Name; const char* DataPath; };
    static constexpr Entry ENTRIES[] = {
        { "tile-ground", "tiles.tile_ground" },
        { "tile-dirt",   "tiles.tile_dirt"   },
        { "tile-ridge",  "tiles.tile_ridge"  },
        { "tile-block",  "tiles.tile_block"  },
        { "tile-coin",   "tiles.tile_coin"   },
    };

    for (const auto& [Name, DataPath] : ENTRIES)
    {
        const auto Path = dataLayer->Data.GetString(DataKey, DataPath);
        if (!Path)
        {
            if (logLayer) logLayer->Log(std::string("[TileWorld] missing tile path for: ") + DataPath);
            continue;
        }

        if (auto Result = Sprite::Load(Path->c_str()))
            TileSprites.emplace(Name, std::move(*Result));
        else if (logLayer)
            logLayer->Log(std::string("[TileWorld] tile sprite load failed: ") + *Path + " — " + Result.error());
    }
}

// ── buildTileMap (private) ─────────────────────────────────────────────────────
// Reads the "tiles" array-of-tables from the "platformerMap" DataLayer dataset
// and populates Walls and Coins.  After both vectors are fully populated,
// builds the BoxCollisionGrid for each set.
void TileWorld::buildTileMap(const char* MapKey)
{
    auto* dataLayer    = ServiceLocator::TryGet<UGEDataLayer>();
    auto* loggingLayer = ServiceLocator::TryGet<LoggingLayer>();

    if (!MapKey || !*MapKey)
    {
        if (loggingLayer) loggingLayer->Log("[TileWorld] map dataset key is empty");
        return;
    }

    if (!dataLayer)
    {
        if (loggingLayer) loggingLayer->Log("[TileWorld] DataLayer not available for tile map");
        return;
    }

    const auto* Rows = dataLayer->Data.GetRows(MapKey, "tiles");
    if (!Rows)
    {
        if (loggingLayer) loggingLayer->Log(std::string("[TileWorld] dataset '") + MapKey + "' has no 'tiles' array");
        return;
    }

    // ── Pass 1: populate Walls and Coins ─────────────────────────────────────
    for (const auto& Row : *Rows)
    {
        const auto xPosRow   = Row.find("x");
        const auto yPosRow   = Row.find("y");
        const auto spriteRow = Row.find("sprite");
        if (xPosRow == Row.end() || yPosRow == Row.end() || spriteRow == Row.end()) continue;

        const auto* xPosition = std::get_if<float>(&xPosRow->second);
        const auto* yPosition = std::get_if<float>(&yPosRow->second);
        const auto* spriteKey = std::get_if<std::string>(&spriteRow->second);
        if (!xPosition || !yPosition || !spriteKey) continue;

        const Sprite* Spr = nullptr;
        if (const auto It = TileSprites.find(*spriteKey); It != TileSprites.end())
            Spr = &It->second;
        else if (loggingLayer)
            loggingLayer->Log(std::string("[TileWorld] unknown tile key '") + *spriteKey + "' in " + MapKey);

        const bool IsCoin = (*spriteKey == "tile-coin");
        auto& Target = IsCoin ? Coins.Tiles : Walls.Tiles;
        Target.emplace_back(*xPosition, *yPosition);
        Target.back().Spr    = Spr;
        Target.back().IsCoin = IsCoin;
    }

    // ── Pass 2: build collision pointer vectors and construct grids ───────────
    static constexpr int TILE_COARSENESS = 160;

    std::vector<BoxCollision*> wallPtrs;
    wallPtrs.reserve(Walls.Tiles.size());
    for (auto& T : Walls.Tiles)
        wallPtrs.push_back(&T.Bounds);
    Walls.CollisionGrid = BoxCollisionGrid(&wallPtrs, TILE_COARSENESS);

    std::vector<BoxCollision*> coinPtrs;
    coinPtrs.reserve(Coins.Tiles.size());
    for (auto& T : Coins.Tiles)
        coinPtrs.push_back(&T.Bounds);
    Coins.CollisionGrid = BoxCollisionGrid(&coinPtrs, TILE_COARSENESS);

    if (loggingLayer)
        loggingLayer->Log("[TileWorld] loaded " + std::to_string(Walls.Tiles.size())
                          + " walls and " + std::to_string(Coins.Tiles.size()) + " coins");
}
