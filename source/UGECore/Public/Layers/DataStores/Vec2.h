#pragma once

// ── Vec2 ──────────────────────────────────────────────────────────────────────
// A small two-component float value stored as a single DataValue so that paired
// quantities (positions, velocities, screen anchors, …) update and notify
// atomically — a single change notification carries both components, so
// subscribers never see half-updated state.
//
// A Vec2 becomes first-class in the DataStore by registering a ValueConverter
// (see DataValue.cpp) so TOML persistence and the Lua Data.* bridge can round
// trip it.  In Lua it marshals to/from a table: { x = <n>, y = <n> }.
struct Vec2
{
    float X = 0.0f;
    float Y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float InX, float InY) : X(InX), Y(InY) {}

    friend constexpr bool operator==(const Vec2&, const Vec2&) = default;
};

