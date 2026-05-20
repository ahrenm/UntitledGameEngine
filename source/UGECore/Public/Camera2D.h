#pragma once
#include <SDL3/SDL.h>

// ── Camera2D ──────────────────────────────────────────────────────────────────
// Describes the 2-D view transform applied when converting world-space positions
// to SDL logical screen-space coordinates.
//
// Coordinate system contract
// --------------------------
//   World space  : Y-up, origin (0, 0) at the bottom-left of the reference area.
//                  X increases rightward; Y increases upward.
//   Screen space : Y-down (SDL convention).  Origin (0, 0) is the top-left corner
//                  of the logical presentation area (RefWidth × RefHeight).
//
// Default camera
// --------------
//   FlipY = true, WorldX = WorldY = 0, Zoom = 1 — equivalent to rendering in
//   reference-pixel screen space but with a Y-up world convention.  Scenes that
//   never modify the camera still work correctly without any coordinate math.
//
// Transform formulas (FlipY = true)
// ----------------------------------
//   screenX      = (worldX - WorldX) * Zoom
//   screenY      = RefHeight - (worldY - WorldY) * Zoom       [point]
//   SDL_FRect.y  = RefHeight - (worldY + rectH - WorldY) * Zoom  [AABB top edge]
//
// Usage — from SceneObject helpers (preferred):
//   SDL_FRect dest = WorldRect(x, y, w, h);   // x,y = bottom-left in world space
//   SDL_FPoint pt  = WorldToScreen(wx, wy);
//
struct Camera2D
{
    float WorldX = 0.0f;  // world-space X of the viewport's left edge
    float WorldY = 0.0f;  // world-space Y of the viewport's bottom edge
    float Zoom   = 1.0f;  // uniform scale (>1 = zoom in; <1 = zoom out)
    bool  FlipY  = true;  // when true, Y-up world → Y-down screen conversion is applied

    // ── Point transforms ──────────────────────────────────────────────────────

    // World position → screen-space SDL_FPoint.
    // RefHeight is the logical presentation height (SDLLayer::RefHeight()).
    [[nodiscard]] SDL_FPoint WorldToScreen(float Wx, float Wy, float RefHeight) const noexcept
    {
        if (FlipY)
            return { (Wx - WorldX) * Zoom,
                     RefHeight - (Wy - WorldY) * Zoom };
        return { (Wx - WorldX) * Zoom, (Wy - WorldY) * Zoom };
    }

    // Screen-space position → world position (inverse of WorldToScreen).
    [[nodiscard]] SDL_FPoint ScreenToWorld(float Sx, float Sy, float RefHeight) const noexcept
    {
        if (FlipY)
            return { Sx / Zoom + WorldX,
                     (RefHeight - Sy) / Zoom + WorldY };
        return { Sx / Zoom + WorldX, Sy / Zoom + WorldY };
    }

    // ── AABB transform ────────────────────────────────────────────────────────

    // World-space AABB → SDL_FRect in screen space.
    // Wx, Wy = bottom-left corner of the box in world space (Y-up convention).
    // W, H   = size in world pixels.
    // RefHeight is the logical presentation height (SDLLayer::RefHeight()).
    [[nodiscard]] SDL_FRect WorldRect(float Wx, float Wy,
                                      float W,  float H,
                                      float RefHeight) const noexcept
    {
        if (FlipY)
            return { (Wx - WorldX) * Zoom,
                     RefHeight - (Wy + H - WorldY) * Zoom,
                     W * Zoom,
                     H * Zoom };
        return { (Wx - WorldX) * Zoom, (Wy - WorldY) * Zoom, W * Zoom, H * Zoom };
    }
};

