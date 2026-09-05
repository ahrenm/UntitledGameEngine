# Sprite System

Three reusable rendering helpers live under `source/UGECore/Public/Sprite/` (include root
`Sprite/`). They own a GPU-backed [`GpuTexture`](../../source/UGECore/Public/Render/GpuTexture.h)
loaded from the [VFS](AssetSystem.md) and draw through the Core `Renderer2D` (the SDL_GPU
textured-quad renderer).

Related: [Animation.md](Animation.md) · [AssetSystem.md](AssetSystem.md)
Key files:
[`Sprite.h`](../../source/UGECore/Public/Sprite/Sprite.h),
[`SpriteSheet.h`](../../source/UGECore/Public/Sprite/SpriteSheet.h),
[`AnimatedSprite.h`](../../source/UGECore/Public/Sprite/AnimatedSprite.h)

---

## Sprite

`<Sprite/Sprite.h>` — owns a single `GpuTexture` loaded from the VFS. `Load()` resolves the
`SDL_GPUDevice` and `PhysFSLayer` through the `ServiceLocator`, so it takes only the asset path.

```cpp
Sprite::Load("assets/…", PaddingH, PaddingV)
    -> std::expected<Sprite, std::string>;
Draw(Renderer2D&, SDL_FRect, FlipMode);
```

Optional `PaddingH` / `PaddingV` shrink the destination rect at draw time while sampling the
full source texture. Query dimensions via `Width()`, `Height()`, `RenderedW()`, `RenderedH()`.

---

## SpriteSheet

`<Sprite/SpriteSheet.h>` — owns a single texture divided into a uniform grid of frames
(left-to-right, top-to-bottom, 0-indexed).

```cpp
SpriteSheet::Load("assets/…", FrameW, FrameH, PaddingH, PaddingV);
Draw(Renderer2D&, FrameIndex, Dest, FlipMode);
```

Query via `FrameCount()`, `FrameW()`, `FrameH()`, `RenderedW()`, `RenderedH()`.

---

## AnimatedSprite

`<Sprite/AnimatedSprite.h>` — stateful animation controller that holds a **non-owning**
`const SpriteSheet*` plus a frame sub-range `[StartFrame, StartFrame+FrameCount)`.

- Call `Update(float DeltaSeconds)` once per frame in `Scene::Update()`.
- Call `Draw(Renderer2D&, Dest, FlipMode)` in `Scene::Draw()`.
- `Reset()` rewinds to the first frame.

The owning scene must ensure the `SpriteSheet` outlives the `AnimatedSprite`.

For a higher-level, TOML-driven registry of named clips with state selection, see
[Animation.md](Animation.md).

