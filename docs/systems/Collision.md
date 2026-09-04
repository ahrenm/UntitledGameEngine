# Collision

Lightweight **AABB** collision helpers used for tile/player collision and a debug overlay.
These are independent of the Box2D-backed [Physics](Physics.md) system — use them for simple
axis-aligned overlap math and broad-phase queries.

Related: [Physics.md](Physics.md) · [SpriteSystem.md](SpriteSystem.md)
Key files:
[`BoxCollision.h`](../../source/UGECore/Public/GameClasses/BoxCollision.h),
[`BoxCollisionRegistry.h`](../../source/UGECore/Public/GameClasses/BoxCollisionRegistry.h),
[`BoxCollisionGrid.h`](../../source/UGECore/Public/GameClasses/BoxCollisionGrid.h)

---

## BoxCollision

AABB helper at `<GameClasses/BoxCollision.h>`.

- **Overlap tests**: `Intersects(Other)` for overlap detection; `OverlapX(Other)` /
  `OverlapY(Other)` return the MTV depth for axis-aligned push-back.
- **Edge accessors**: `Left()`, `Top()`, `Right()`, `Bottom()`, `CenterX()`, `CenterY()`.
- **Registration state**: `IsRegistered()`, `RegistryId()`.
- **`Enabled` flag**: when `false`, all intersection tests return `false` — toggle freely at
  runtime (e.g. to deactivate a collected coin).

### UserData Convention

Every instantiator of a `BoxCollision` member **must** set `Bounds.UserData = this` (or
equivalent) immediately after construction, so `BoxCollisionGrid::Intersects()` results can be
cast back to the owning type at the call site (e.g. `static_cast<Tile*>(hit->UserData)`).

`UserData` is intentionally **not** propagated by copy or move — it is the identity of the
owning object and must always be stamped by that object. Classes that call `UpdateCollision()`
should set `UserData` inside that helper (see `Tile::UpdateCollision()`).

### Auto-Registration

The value constructor `BoxCollision(X, Y, W, H [, Label])` registers with
`BoxCollisionRegistry::Active()` when one is set (i.e. after `SDLLayer::InitCollisionHooks()` —
called automatically in `SDLLayer::Create()`).

- **Copy** construction does **not** register (copies are for temporary collision math only).
- **Move** construction transfers the existing registration to the new address.
- **Destruction** automatically deregisters.
- **Default** construction does not register — use default + assignment for temporary boxes.

---

## BoxCollisionRegistry

Non-owning debug-overlay registry at `<GameClasses/BoxCollisionRegistry.h>`. Owned by
`SDLLayer` (as `m_collisionRegistry`).

- `SDLLayer::ShowCollisionBoxes(bool)` / `IsShowingCollisionBoxes()` toggle the overlay rendered
  in `Draw()`.
- Explicit opt-in (for boxes that should not auto-register):
  `SDLLayer::RegisterCollision(Box*, Label)` → `CollisionHandle` (RAII, deregisters on
  destruction).
- Access the registry directly via `SDLLayer::CollisionRegistry()`.
- Lua: `SDL.ShowCollision()` toggles the overlay and logs the state.

---

## BoxCollisionGrid

Broad-phase uniform spatial grid for AABB queries at `<GameClasses/BoxCollisionGrid.h>`.

**STATIC after construction** — build once with a `std::vector<BoxCollision*>` and a coarseness
value (cell side-length in reference-space units). Query with
`grid.Intersects(&playerBox, hits)`, which fills `hits` with candidate overlapping boxes; then
do narrow-phase against the results.

The grid stores **non-owning** pointers — the source vector and `BoxCollision` objects must
outlive the grid. Used by `TileSet::CollisionGrid` inside `TileWorld`.

