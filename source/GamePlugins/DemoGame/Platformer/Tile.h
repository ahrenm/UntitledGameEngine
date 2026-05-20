#pragma once
#include <SDL3/SDL.h>
#include <GameClasses/BoxCollision.h>
#include <Physics/PhysicsBodyHandle.h>

class Sprite;

// ── Tile ──────────────────────────────────────────────────────────────────────
// Represents a single square tile in the game world.
//
// Coordinate convention: Y-up world pixels.
//   X = left edge,  Y = BOTTOM edge of the tile (Y=0 → sitting on the world floor).
//   WIDTH × HEIGHT extend right and upward from (X, Y).
//
// Rendering: SceneObject::WorldRect(T.X, T.Y, T.SIZE_W, T.SIZE_H) converts the
//   AABB to an SDL screen-space rect via the active Camera2D transform.
//
// Collision: Bounds.Y = bottom edge; Bounds.Y + Bounds.H = top edge (MaxY).
//   Use Bounds.MinY() / Bounds.MaxY() in physics code for clarity.
struct Tile
{
    // ── Construction ──────────────────────────────────────────────────────────
    Tile() { UpdateCollision(); }

    Tile(float X, float Y, float W = 160.0f, float H = 160.0f)
        : X(X), Y(Y), SIZE_W(W), SIZE_H(H)
    {
        UpdateCollision();
    }

    // ── World-space position and size ─────────────────────────────────────────
    float X      = 0.0f;    // left edge in Y-up world pixels
    float Y      = 0.0f;    // BOTTOM edge in Y-up world pixels (Y=0 = world floor)
    float SIZE_W = 160.0f;  // tile width  in world pixels
    float SIZE_H = 160.0f;  // tile height in world pixels

    // ── Rendering ─────────────────────────────────────────────────────────────
    SDL_Color     Color = { 34, 139, 34, 255 };  // forest-green colour fallback
    const Sprite* Spr   = nullptr;               // non-owning; may be nullptr

    // ── Gameplay flags ────────────────────────────────────────────────────────
    // When true the tile is a collectible coin: passable and removed on player
    // overlap via the CollectCoins callback in PlatformerScene.
    bool IsCoin    = false;
    bool IsVisible = true;

    // Show/Hide control both visibility and collision together.
    void Show() { IsVisible = true;  Bounds.Enabled = true;  }
    void Hide() { IsVisible = false; Bounds.Enabled = false; }

    // ── Collision ─────────────────────────────────────────────────────────────
    BoxCollision     Bounds;  // AABB kept in sync with X/Y/SIZE_W/SIZE_H (Y-up, bottom-left)
    PhysicsBodyHandle Body;   // Box2D static/sensor body (owned; destroyed with scene)

    // Rebuilds Bounds from the current X, Y, SIZE_W, SIZE_H values.
    // Also re-stamps Bounds.UserData = this so grid queries can recover the Tile.
    // Call this after any positional or size change at runtime.
    void UpdateCollision()
    {
        Bounds = { X, Y, SIZE_W, SIZE_H };
        Bounds.UserData = this;
    }
};
