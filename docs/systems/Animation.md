# Animation

`AnimationControllerBase` is a generic, **TOML-driven** animation controller for game objects.
It manages a named registry of `SpriteSheet` + `AnimatedSprite` pairs and renders the currently
active clip through a bound `Renderer2D`. Derive from it in a game plugin to add state-driven
animation selection (e.g. idle/walk/jump).

Related: [SpriteSystem.md](SpriteSystem.md) · [DataStore.md](DataStore.md) ·
[SceneSystem.md](SceneSystem.md)
Key file:
[`AnimationControllerBase.h`](../../source/UGECore/Public/Animation/AnimationControllerBase.h)

---

## Position in the Stack

`AnimationControllerBase` derives from `GameObjectBase`, so inside it you get the protected
layer accessors (`GetDataLayer()`, `GetSDLLayer()`, …). It sits **above** the raw
[Sprite system](SpriteSystem.md): each named animation entry owns a heap-allocated
`SpriteSheet` (stable address) with an `AnimatedSprite` pointing into it — so `AnimatedSprite`
pointers never dangle across map insertions.

---

## Lifecycle

1. Construct with the `Renderer2D*` that will be used for all `Draw` calls.
2. Call `LoadAnimation()` once per clip (reads from `UGEDataLayer`).
3. Each frame: call `Update()` then `Tick(DeltaTime)`; call `Draw(Rect, Flip)` to render.

```cpp
explicit AnimationControllerBase(Renderer2D* Renderer);
```

---

## Loading Clips

```cpp
void LoadAnimation(const char* DatasetKey,
                   const std::string& SectionPrefix,   // e.g. "character.walk"
                   const std::string& AnimName);
```

Reads animation config from the TOML dataset `DatasetKey` under `SectionPrefix` and registers
the clip under `AnimName`. Expected TOML keys (all relative to `SectionPrefix`):

| Key | Meaning |
|---|---|
| `sprite` | VFS path to the sprite sheet texture. |
| `frame_w` / `frame_h` | Frame dimensions in pixels. |
| `padding_h` | Horizontal padding applied at draw time. |
| `start_frame` | First frame index of the clip within the sheet. |
| `frame_count` | Number of frames in the clip. |
| `frame_duration` | Seconds per frame. |

Datasets under `assets/DATA/` are parsed into the [DataStore](DataStore.md) at layer creation —
so clips are available without any manual file I/O.

---

## Playback Control

| Method | Purpose |
|---|---|
| `SetCurrentAnimation(AnimName)` | Select the active clip. |
| `ResetAnimation(AnimName)` | Rewind a clip to its first frame. |
| `IsAnimationValid(AnimName)` | `bool` — whether the clip loaded successfully. |
| `GetRenderSize(AnimName)` | `{RenderedW, RenderedH}` for the clip's sheet (`{0,0}` if invalid). |

---

## Frame Loop Contract

| Method | Role |
|---|---|
| `Update()` | **Reserved for `AnimationNotify`** — future per-frame event callbacks that inform game state (e.g. footstep sounds, hit-detection windows) will fire here, before `Tick()`. Default no-op; override in a derived class. |
| `Tick(float DeltaTime)` | Advances the active animation's frame timer by `DeltaTime` seconds. |
| `Draw(const SDL_FRect& Rect, SDL_FlipMode = SDL_FLIP_NONE)` | Draws the active frame scaled to fit `Rect`. |

---

## Deriving a Controller

Subclass `AnimationControllerBase`, load your clips in the constructor (or an init method), and
override `Update()` (and optionally add state logic that calls `SetCurrentAnimation()`) to drive
which clip plays based on game state. Because `SpriteSheet`s are heap-allocated, controller
instances may be freely stored in scene-owned containers.

