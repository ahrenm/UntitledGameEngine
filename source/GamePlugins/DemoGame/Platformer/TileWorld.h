#pragma once
#include <GameClasses/BoxCollisionGrid.h>
#include <Physics/PhysicsBodyHandle.h>
#include <Sprite/Sprite.h>
#include <Tile.h>
#include <string>
#include <unordered_map>
#include <vector>

// ── TileSet ───────────────────────────────────────────────────────────────────
// Pairs a tile vector with its matching broad-phase collision grid.
// The grid holds non-owning BoxCollision* pointers into Tiles; Tiles must not
// reallocate after the grid is built.
//
// MergedBodies owns the Box2D static bodies for wall tiles.  Instead of one
// body per tile, horizontally-adjacent coplanar tiles are fused into a single
// wide body — this eliminates the internal-edge "ghost collision" where a
// dynamic body catches on the seam between adjacent static boxes.
struct TileSet
{
    std::vector<Tile>             Tiles;
    BoxCollisionGrid              CollisionGrid;
    std::vector<PhysicsBodyHandle> MergedBodies;  // merged wall bodies (one per contiguous run)
};

// ── TileWorld ─────────────────────────────────────────────────────────────────
// Owns the two tile sets and the sprite atlas that make up the platformer level.
//
// Walls — solid tiles that block traversal (floors, ceilings, platforms).
// Coins — collectible tiles that are passable and removed on player overlap.
//
// Call Build() once after the data layer is available to populate all members.
// DataKey is the platformer dataset name (e.g. "platformerData").
struct TileWorld
{
    std::unordered_map<std::string, Sprite> TileSprites;
    TileSet Walls;
    TileSet Coins;

    void Build(const char* DataKey, const char* MapKey);

private:
    void buildTileSprites(const char* DataKey);
    void buildTileMap(const char* MapKey);
    void buildPhysicsBodies();
};
