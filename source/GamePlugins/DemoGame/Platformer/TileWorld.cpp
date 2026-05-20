#include "TileWorld.h"
#include <Layers/UGEDataLayer.h>
#include <Layers/LoggingLayer.h>
#include <Layers/PhysicsLayer.h>
#include <ServiceLocator.h>
#include <Sprite/Sprite.h>
#include <algorithm>
#include <cmath>

// ── Build ─────────────────────────────────────────────────────────────────────
void TileWorld::Build(const char* DataKey, const char* MapKey)
{
    buildTileSprites(DataKey);
    buildTileMap(MapKey);
    buildPhysicsBodies();
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
        const std::string Key = std::string(DataKey) + "." + DataPath;
        const std::string* Path = nullptr;
        if (const DataValue* V = dataLayer->Store.Get(Key))
            Path = V->TryAs<std::string>();
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

    const RowsView Rows = dataLayer->Store.RowsView(std::string(MapKey) + ".tiles");
    if (Rows.Size() == 0)
    {
        if (loggingLayer) loggingLayer->Log(std::string("[TileWorld] dataset '") + MapKey + "' has no 'tiles' array");
        return;
    }

    // ── Pass 1: populate Walls and Coins ─────────────────────────────────────
    for (const auto& Row : Rows)
    {
        const auto xPosition = Row.Get<float>("x");
        const auto yPosition = Row.Get<float>("y");
        const auto spriteKey = Row.Get<std::string>("sprite");
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

    if (loggingLayer)
        loggingLayer->Log("[TileWorld] loaded " + std::to_string(Walls.Tiles.size())
                          + " walls and " + std::to_string(Coins.Tiles.size()) + " coins");
}

// ── buildPhysicsBodies (private) ──────────────────────────────────────────────
// Wall tiles: merged into single wide bodies per contiguous horizontal run.
// Eliminates the ghost-collision / internal-edge problem where a dynamic body
// catches on the seam between adjacent static boxes at the same Y.
//
// Coin tiles: individual sensor bodies kept per-tile so UserData pointer
// comparisons in PlatformerScene::onContact() can identify the collected tile.
void TileWorld::buildPhysicsBodies()
{
    auto* physics = ServiceLocator::TryGet<PhysicsLayer>();
    if (!physics || !physics->HasPhysicsWorld())
    {
        if (auto* log = ServiceLocator::TryGet<LoggingLayer>())
            log->Log("[TileWorld] buildPhysicsBodies: no active physics world — call InitPhysics() before Build()");
        return;
    }

    // ── Wall tiles: sort then merge adjacent coplanar runs ────────────────────
    {
        // Sort by (Y asc, H asc, X asc) to group rows before scanning for runs.
        std::vector<const Tile*> sorted;
        sorted.reserve(Walls.Tiles.size());
        for (const auto& T : Walls.Tiles) sorted.push_back(&T);
        std::sort(sorted.begin(), sorted.end(), [](const Tile* a, const Tile* b) {
            if (a->Y       != b->Y)       return a->Y       < b->Y;
            if (a->SIZE_H  != b->SIZE_H)  return a->SIZE_H  < b->SIZE_H;
            return a->X < b->X;
        });

        Walls.MergedBodies.reserve(sorted.size());

        for (size_t i = 0; i < sorted.size(); )
        {
            const float startX = sorted[i]->X;
            const float rowY   = sorted[i]->Y;
            const float rowH   = sorted[i]->SIZE_H;
            float       runW   = sorted[i]->SIZE_W;

            // Extend the run while the next tile is coplanar and directly adjacent.
            size_t j = i + 1;
            while (j < sorted.size()
                && sorted[j]->Y      == rowY
                && sorted[j]->SIZE_H == rowH
                && std::abs(sorted[j]->X - (startX + runW)) < 0.5f)
            {
                runW += sorted[j]->SIZE_W;
                ++j;
            }

            // One merged body per run — no per-tile UserData needed for walls.
            Walls.MergedBodies.push_back(physics->CreateBody({
                .X        = startX,
                .Y        = rowY,
                .W        = runW,
                .H        = rowH,
                .Type     = BodyType::Static,
                .Friction = 0.3f,
                .UserData = nullptr
            }));

            i = j;
        }

        if (auto* log = ServiceLocator::TryGet<LoggingLayer>())
            log->Log("[TileWorld] merged " + std::to_string(Walls.Tiles.size())
                     + " wall tiles into " + std::to_string(Walls.MergedBodies.size())
                     + " physics bodies");
    }

    // ── Coin tiles: individual sensor bodies ─────────────────────────────────
    for (auto& T : Coins.Tiles)
    {
        T.Body = physics->CreateBody({
            .X        = T.X,
            .Y        = T.Y,
            .W        = T.SIZE_W,
            .H        = T.SIZE_H,
            .Type     = BodyType::Sensor,
            .UserData = &T
        });
    }
}
