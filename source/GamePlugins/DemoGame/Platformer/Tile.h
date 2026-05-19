#pragma once
#include <SDL3/SDL.h>
#include <GameClasses/BoxCollision.h>

class Sprite;

// ── Tile ──────────────────────────────────────────────────────────────────────
// Represents a single square tile in the game world.
//
// Positions and sizes are expressed in the scene's reference-resolution
// coordinate space (1600 × 1200).  Tick() applies a uniform scale factor
// before passing the destination rect to SDL.
//
// Rendering priority:
//   1. If Spr != nullptr  →  calls Spr->Draw(Renderer, Dest)
//   2. Otherwise          →  filled rectangle using Color (placeholder)
//
// Collision is exposed via the Bounds member (BoxCollision).  Call
// UpdateCollision() after modifying X, Y, SIZE_W or SIZE_H at runtime.
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
    float X      = 0.0f;    // reference-space left edge
    float Y      = 0.0f;    // reference-space top edge
    float SIZE_W = 160.0f;  // tile width  in reference pixels
    float SIZE_H = 160.0f;  // tile height in reference pixels

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
    BoxCollision Bounds;  // kept in sync with X / Y / SIZE_W / SIZE_H

    // Rebuilds Bounds from the current X, Y, SIZE_W, SIZE_H values.
    // Also re-stamps Bounds.UserData = this so grid queries can recover the Tile.
    // Call this after any positional or size change at runtime.
    void UpdateCollision()
    {
        Bounds = { X, Y, SIZE_W, SIZE_H };
        Bounds.UserData = this;
    }
};
