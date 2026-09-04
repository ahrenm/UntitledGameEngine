# Physics

`PhysicsLayer` (load order **4.1**, just after `SDLLayer` 4.0) wraps a single **Box2D 3.x**
world. All Box2D types are confined to `PhysicsLayer.cpp` behind a private `Impl` pimpl; game
code interacts only through the public handle and data-tag API.

Related: [FrameLoop.md](../architecture/FrameLoop.md) · [SceneSystem.md](SceneSystem.md) ·
[DataStore.md](DataStore.md) · [LuaApiReference.md](../reference/LuaApiReference.md)
Key files:
[`PhysicsLayer.h`](../../source/UGECore/Public/Layers/PhysicsLayer.h),
[`PhysicsTypes.h`](../../source/UGECore/Public/Physics/PhysicsTypes.h),
[`PhysicsBodyHandle.h`](../../source/UGECore/Public/Physics/PhysicsBodyHandle.h)

---

## Frame Role Split

| Phase | Work |
|---|---|
| `Update()` | `b2World_Step` + contact-event dispatch + DataLayer state sync. Runs **after** `SDLLayer::Update()` (scene logic / impulse-setting) so physics integrates this frame's impulses before rendering. |
| `Draw()` | `drawDebug()` only. Runs **after** `SDLLayer::Draw()` (scene sprites), so physics AABBs composite on top of game graphics and beneath RmlUi. |

---

## World Lifecycle

```cpp
void  InitPhysics(const PhysicsWorldCreateParams& Params = {});
void  ShutdownPhysics();
bool  HasPhysicsWorld() const;
float PixelsPerMeter()  const;
```

Gravity and pixels-per-metre are **not** passed to `InitPhysics()` — they are always read from
the `physics.*` transient tag branch (seeded from `LaunchSettings`, optionally overridden by the
scene before calling `InitPhysics()`).

`PhysicsWorldCreateParams`:

| Field | Default | Meaning |
|---|---|---|
| `SubSteps` | `8` | Box2D velocity sub-steps per tick (higher = more accurate). |
| `EnableSleeping` | `true` | Allow static bodies to sleep when undisturbed. |

---

## Bodies

Create bodies with `PhysicsBodyHandle CreateBody(const BodyDef& Def)`.

### BodyDef

`X, Y` = bottom-left corner in **Y-up world pixels**; `W, H` = size in world pixels.

| Field | Default | Notes |
|---|---|---|
| `X` / `Y` | `0` | Bottom-left corner (Y-up). |
| `W` / `H` | `0` | Size in world pixels. |
| `Type` | `Static` | See `BodyType` below. |
| `Density` | `1.0` | |
| `Friction` | `0.3` | |
| `Restitution` | `0.0` | Bounciness. |
| `UserData` | `nullptr` | Stored on the Box2D shape so contact events can identify the owner. **Must outlive the body.** |
| `FixedRotation` | `true` | Prevents rotation (recommended for characters). |

### BodyType

| Value | Behaviour |
|---|---|
| `Static` | Immovable; terrain and walls. |
| `Dynamic` | Gravity + collision response; player and projectiles. |
| `Kinematic` | Velocity-driven, no gravity; moving platforms. |
| `Sensor` | Static sensor shape; generates contact events, no collision response. |

---

## PhysicsBodyHandle

An **RAII** handle to a single body. Destroys the body when it goes out of scope by calling
`PhysicsLayer::releaseBody()`.

### ⚠ Ownership Contract

A `PhysicsBodyHandle` **MUST** be stored as a non-static member of the scene (or a scene-owned
object such as `PlatformerCharacter` or `Tile`) that created it. It **MUST NOT** be stored in:

- global or static variables,
- `UGEDataLayer` / AppState, or
- any container with a lifetime longer than the active scene.

`SceneManagerLayer` destroys the scene (and all its handles) **then** calls
`PhysicsLayer::ShutdownPhysics()`. Handles that outlive their scene would call `releaseBody()`
against an already-destroyed world. A `weak_ptr` alive-token guards this: when `PhysicsLayer` is
destroyed, expired handles skip `releaseBody()` safely.

Moving handles into `std::vector<PhysicsBodyHandle>` is safe — the move constructor transfers
ownership correctly. `IsValid()` returns `false` for default-constructed, moved-from, or
shut-down handles; all other methods silently no-op on an invalid handle.

### Handle API

| Method | Units / Notes |
|---|---|
| `SetPosition(Wx, Wy)` / `GetPosition()` | World pixels, Y-up, bottom-left origin. |
| `SetLinearVelocity(Vx, Vy)` / `GetLinearVelocity()` | World pixels/second. |
| `GetMass()` | Box2D kg. |
| `ApplyImpulse(IxNs, IyNs)` | Linear impulse (N·s) at the body centre. |
| `SetEnabled(bool)` | Enable/disable the body. |
| `IsGrounded(MinNormalY = 0.7f)` | `true` if a contact normal is ≥ `MinNormalY` from horizontal (0.7 ≈ 45°). Queries live manifolds — call after `PhysicsLayer::Update()`. |

**Impulse vs. velocity**: `ApplyImpulse` produces true mass-scaled momentum; `SetLinearVelocity`
bypasses dynamics. To achieve an exact pixel-space velocity change `ΔvPxS` on one axis:

```cpp
handle.ApplyImpulse(handle.GetMass() * ΔvPxS / PixelsPerMeter(), 0.0f);
```

---

## Contact Events

Register a callback with `SetContactCallback(std::function<void(const ContactEvent&)>)`. Sensor
contacts only (Box2D 3.x raises begin/end events for sensor shapes).

```cpp
struct ContactEvent {
    void*        UserDataA; // BodyDef.UserData of shape A's body
    void*        UserDataB; // BodyDef.UserData of shape B's body
    ContactPhase Phase;     // Begin | End
};
```

---

## DataLayer State Sync

`EnableStateSync(PhysicsBodyHandle& Handle, std::string_view KeyPrefix)` mirrors a body's state
into the DataStore each `Update()` under the given key prefix — useful for binding HUD/UI or Lua
to live body state.

---

## Runtime-Tunable World Parameters

Canonical tag definitions are in
[`PhysicsTypes.h`](../../source/UGECore/Public/Physics/PhysicsTypes.h). Parameters live as
`physics.*` **transient** DataStore keys.

### LIVE — applied immediately

- `physics.gravity.x` / `physics.gravity.y` (m/s², Y-negative = down). Changing either while a
  world is active updates the running Box2D world immediately via a `DataBinding` →
  `b2World_SetGravity()`. Tune with `Data.Set('physics.gravity.y', -20)` **or** the sugar
  `Physics.SetGravity(0, -20)` (both write the same keys). The platformer's jump math
  (`PlatformerCharacter`) also binds `physics.gravity.y`, so it stays consistent.

### INIT-ONLY — applied on next InitPhysics()

- `physics.pixels_per_meter` (px/m). Box2D bakes body sizes at `CreateBody()` time, so PPM
  changes apply on the **next** `InitPhysics()`; `Physics.SetPixelsPerMeter()` refuses while a
  world is live and logs a warning.

### Debug Overlay

- `debug.show_collision` (int: `1` = visible, `0` = hidden) — a **shared** tag used by both
  `PhysicsLayer` and `SDLLayer` so a single toggle activates the physics-body debug overlay.
  Write via `Physics.DebugDraw(true/false)` or `SDL.ShowCollision()`.

### Scope Note

This is **engine-scoped** physics — the old game-scoped `plat2d.physics.*` namespace was
removed. Platformer *movement* tunables (not Box2D world params) live under `plat2d.movement.*`
(`plat2d.movement.speed`, `plat2d.movement.jump_height`; see
[`PlatformerStateTags.h`](../../source/GamePlugins/DemoGame/Platformer/PlatformerStateTags.h)).

---

## Lua Table

`Physics.InitPhysics()`, `Physics.ShutdownPhysics()`, `Physics.HasWorld()` → bool,
`Physics.GetBodyCount()` → int, `Physics.ShowCollision()` (toggle overlay),
`Physics.DebugDraw(on)`, `Physics.SetGravity(gx, gy)`, `Physics.SetPixelsPerMeter(ppm)`.
Full listing in [LuaApiReference.md](../reference/LuaApiReference.md).

