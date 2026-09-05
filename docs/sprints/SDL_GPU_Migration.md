# SDL_GPU Migration — Sprint Plan

> Migration of UGE's rendering stack from the deprecated SDL 2D renderer (`SDL_Renderer`) to the
> **SDL_GPU** device/swapchain pipeline, including RmlUi's `RenderInterface_SDL_GPU`, a Core
> `Renderer2D`, GPU-texture asset loading, and a full GPU upgrade of the TeaPot3D demo.
>
> Companion reading: [AGENTS.md](../../AGENTS.md),
> [docs/architecture/FrameLoop.md](../architecture/FrameLoop.md),
> [docs/systems/SpriteSystem.md](../systems/SpriteSystem.md),
> [docs/systems/Rendering3D.md](../systems/Rendering3D.md).

## Background

RmlUi has **removed** the SDL 2D renderer backend (`RenderInterface_SDL` / `RmlUi_Renderer_SDL.cpp`).
Its changelog deprecates the `SDLrenderer` and steers SDL3 users to the **SDL GPU** renderer, which
is the modern, fully supported path. The SDL 2D renderer was hardware-accelerated but acted as a
lowest-common-denominator layer: it could not express the modern `Rml::RenderInterface` features
(layer compositing, filters, clip masks, custom shaders), and those effects fell back to the CPU or
were unavailable. SDL_GPU is SDL3's cross-platform GPU abstraction (Vulkan / D3D12 / Metal under one
API), giving RmlUi — and us — a direct, capable GPU pipeline.

Because the **entire engine** currently draws through one `SDL_Renderer` (background, sprites,
scenes, the TeaPot software renderer, and RmlUi), RmlUi can no longer piggyback on it. All draw
paths and the frame loop must move to SDL_GPU together.

## Design Principle (drives every decision below)

**Core owns the plumbing; plugins keep raw SDL access.** The Core engine (`SDLLayer` + new render
modules) owns the `SDL_GPUDevice`, swapchain acquisition, the per-frame command buffer,
viewport/projection (letterbox), and asset→GPU upload. It exposes those raw SDL_GPU handles so Game
Plugins retain **full low-level access** (e.g. `Render3DLayer` records its own render pass and
pipelines). Core additionally offers a convenience `Renderer2D` for the common case (sprites,
background) that plugins may use or bypass.

### Global constraints (every phase)

- Follow `AGENTS.md` conventions (PascalCase public / `m_`camelCase private; `Create()` +
  `std::expected`; `REGISTER_*` in headers).
- VFS-only I/O through `PhysFSLayer`; asset paths begin `assets/...`.
- Build (agent/terminal): prepend MinGW `bin` to `PATH`, then
  `cmake --build C:\Users\ahren\CLionProjects\untitled\cmake-build-debug --target all -j 62`.
- **Broken builds between phases are expected and acceptable.** Each phase lists its expected build
  state; the tree returns to green by the end of Phase 6.

---

## Dependency Graph

```
P0  Shader authoring + manual precompile  (GREEN)
 └─► P1  GPU device + frame orchestration in SDLLayer   (BREAKS build)
       ├─► P2  Renderer2D + GPU-texture asset loading   (BREAKS/partial)
       │     └─► P3  Sprites / SpriteSheet / background port  (GREEN core)
       ├─► P4  RmlUi SDL_GPU render interface            (GREEN with P3)
       └─► P5  TeaPot3D full GPU upgrade (plugin)        (GREEN)
             └─► P6  Plumbing cleanup + docs             (GREEN, final)
```

---

## P0 — Shader authoring & manual precompile · build: GREEN

**Goal**: Ship basic, precompiled shaders for the Core `Renderer2D` (and later TeaPot) as committed
asset binaries. Automation is explicitly deferred.

**Create** (`assets/shaders/`):

- `src/sprite.vert.hlsl`, `src/sprite.frag.hlsl` — textured quad (position + UV in, sampled texture
  out; one uniform block for the MVP/projection matrix).
- `src/color.frag.hlsl` — flat/vertex-color fragment (for the background fill + TeaPot colored path).
- Precompiled outputs committed alongside: `sprite.vert.spv`, `sprite.frag.spv`, `color.frag.spv`
  (SPIR-V is the primary target on Windows/Vulkan). Optionally `.dxil` / `.msl` variants for future
  portability.
- `assets/shaders/README.md` — records the exact manual compile commands (see below) and the
  deferred-automation note.

**Manual precompile (documented, run by hand for now)**:

- Use `SDL_shadercross` (or `glslangValidator` / `dxc`) to translate HLSL → SPIR-V, e.g.
  `shadercross sprite.vert.hlsl -o sprite.vert.spv`.
- Note the SDL_GPU resource-binding rules in the README (sampler/uniform register spaces) so
  hand-authored shaders match the `Renderer2D` pipeline layout.

**Constraints**:

- Shaders load via the VFS like any other asset (`assets/shaders/...`), through `PhysFSLayer`.
- Keep uniform layouts minimal and documented; `Renderer2D` and shaders must agree on binding
  indices.

**Acceptance**: `.spv` binaries exist under `assets/shaders/` and are picked up by
`pack_assets.cmake`; a short doc block lists reproducible compile commands. No engine code depends
on them yet.

**Deferred**: a CMake custom-command / CI step to auto-compile shaders on build.

---

## P1 — GPU device + frame orchestration · build: BREAKS

**Goal**: Replace `SDL_Renderer` ownership in `SDLLayer` with an `SDL_GPUDevice`, swapchain, and a
shared per-frame command buffer. Expose raw handles for plugins.

**Modify**:

- `source/UGECore/Public/Layers/SDLLayer.h` — drop `UniqueRenderer` / `SDLRendererDeleter`; add
  `SDL_GPUDevice*` ownership and accessors: `Device()`, `CommandBuffer()`, `SwapchainTexture()`,
  `SwapchainWidth/Height()`, `Frame()` (aggregate `GpuFrameContext`), and `IsFrameActive()`. Keep
  `RefWidth/RefHeight`. (Depth buffer is **not** created in Core — the TeaPot plugin owns its own in
  P5.)
- `source/UGECore/Private/Layers/SDLLayer.cpp` — `Create()`: create window (no renderer),
  `SDL_CreateGPUDeviceWithProperties` (SPIRV/DXIL/MSL flags), `SDL_ClaimWindowForGPUDevice`.
  `BeginFrame()`: `SDL_AcquireGPUCommandBuffer` + `SDL_WaitAndAcquireGPUSwapchainTexture`, run a
  clear render pass (`LOAD_OP_CLEAR`). `EndFrame()`: `SDL_SubmitGPUCommandBuffer`. Handle minimize
  (null swapchain → cancel).
- Introduce a small `GpuFrameContext` (command buffer, swapchain texture, dims) surfaced via
  `SDLLayer` for other layers/plugins.

**Constraints**:

- Only **one** command buffer per frame, owned by `SDLLayer`; all `Draw()` passes record into it,
  submitted once in `EndFrame()`.
- Remove `SDL_SetRenderLogicalPresentation`; capture `RefWidth/RefHeight` for P2's projection.
- Preserve the existing move-ctor/dtor RAII discipline (device release, window destroy, `SDL_Quit`).

**Acceptance**: Engine boots to a cleared swapchain (colored background) each frame. Everything using
`SDL_Renderer` (sprites, RmlUi, TeaPot, scenes) **fails to compile** — expected; fixed in P2–P5.

**Build state**: RED (widespread `SDL_Renderer` references unresolved).

---

## P2 — Renderer2D + GPU-texture asset loading · build: partial

**Goal**: Core convenience renderer and a GPU texture type replacing `SDL_Texture`, with
viewport/projection (letterbox) handled here.

**Create** (`source/UGECore/{Public,Private}/Render/`):

- `GpuTexture.h/.cpp` — RAII wrapper over `SDL_GPUTexture` + dimensions; upload helper
  (`SDL_Surface` → transfer buffer → `SDL_UploadToGPUTexture`).
- `Renderer2D.h/.cpp` — owns the textured-quad + color pipelines (loads P0 `.spv` via VFS), a
  per-frame render pass (`LOAD_OP_LOAD`), and `DrawTexture(GpuTexture&, srcRect, dstRect, flip)` /
  `DrawColoredQuad(...)`. Computes the **viewport + orthographic projection** from
  `RefWidth/RefHeight` vs swapchain size (letterbox), exposed via `Projection()` and
  `SetViewport()`.

**Modify**:

- `source/UGECore/Private/Layers/SDLLayer.cpp` — replace `LoadTextureFromPhysFS` (was
  `IMG_LoadTexture_IO`, needs a renderer) with `IMG_Load_IO` → `SDL_Surface` → `GpuTexture`. Own a
  `Renderer2D`; background blit uses it.

**Constraints**:

- `Renderer2D` records into `SDLLayer`'s shared command buffer; never acquires/submits its own.
- Letterbox math lives in one place (`Renderer2D`) and is queryable by plugins.
- Texture uploads batch into a copy pass at frame start where possible.

**Acceptance**: Background image renders via `Renderer2D` at correct letterboxed aspect. `GpuTexture`
smoke-load of a PNG succeeds.

**Build state**: RED→partial (core renders; sprite/scene/RmlUi/TeaPot still pending).

---

## P3 — Sprite / SpriteSheet / scene port · build: GREEN (core)

**Goal**: Move the sprite abstractions and scene plumbing off `SDL_Renderer` onto `Renderer2D` /
`GpuTexture`.

**Modify**:

- `source/UGECore/{Public,Private}/Sprite/Sprite.*`, `SpriteSheet.*` — hold `GpuTexture`;
  `Draw(Renderer2D&, dstRect, flip)` (+ `frameIndex` / `srcRect` for sheets) replacing
  `SDL_RenderTexture(Rotated)`.
- `source/UGECore/{Public,Private}/Animation/AnimationControllerBase.*` — swap the bound
  `SDL_Renderer*` for `Renderer2D*`.
- `source/UGECore/Public/GameClasses/SceneObject.h`, `source/UGECore/Private/SceneRegistry.cpp`,
  `source/UGECore/Private/Layers/SceneManagerLayer.cpp` — thread `Renderer2D&` (+ raw
  `SDL_GPUDevice*` / window for plugins) instead of `SDL_Renderer*`.
- `source/UGECore/Private/Layers/PhysicsLayer.cpp` — the Box2D debug-draw path uses the SDL 2D
  renderer (`Sdl->Renderer()` + line/poly draws); port it to `Renderer2D` primitives (or gate it
  off until ported).
- DemoGame scenes (`StateAssetsDemoScene`, etc.) — update ctor signatures and draw calls.

**Constraints**: Keep the `Camera2D` transform semantics (Y-flip, zoom) — fold into the `Renderer2D`
projection / view.

**Acceptance**: DemoGame sprites / animations render correctly; scene load / unload ordering
preserved.

**Build state**: GREEN for Core + DemoGame (RmlUi and TeaPot still to switch).

---

## P4 — RmlUi SDL_GPU render interface · build: GREEN

**Goal**: Point RmlUi at `RenderInterface_SDL_GPU`, sharing Core's command buffer / swapchain.

**Modify**:

- `source/UGECore/CMakeLists.txt` — backend sources → `RmlUi_Platform_SDL.cpp` +
  `RmlUi_Renderer_SDL_GPU.cpp` (drop the GL3 / SDL renderer entries).
- `source/UGECore/Public/Layers/RmlUILayer.h` — `#include "RmlUi_Renderer_SDL_GPU.h"`; member
  `RenderInterface_SDL_GPU` constructed with `(device, window)`; ctor takes device / window from
  `SDLLayer`.
- `source/UGECore/Private/Layers/RmlUILayer.cpp` — `Create()` fetches `Device()` / `Window()`;
  `BeginFrame()` calls `renderInterface.BeginFrame(cmdBuf, swapchainTex, w, h)`, `EndFrame()` calls
  `renderInterface.EndFrame()`; delete the old `SDL_SetRenderViewport` / `SDL_RenderClear` hacks and
  the `Frame(SDL_Renderer*, SDL_Texture*)` overload.

**Constraints**:

- RmlUi's SDL_GPU shaders are self-contained (embedded SPIR-V / DXIL / MSL) — no asset work needed
  for UI.
- RmlUi composites with `LOAD` (no clear); ensure it runs **after** background / sprite / 3D passes
  in `Draw()` order.

**Acceptance**: RmlUi documents render over the game content; input, data-models, and debugger
unaffected.

**Build state**: GREEN (Core + UI + DemoGame). TeaPot still on the CPU path (temporarily disabled or
stubbed).

---

## P5 — TeaPot3D full GPU upgrade · build: GREEN

**Goal**: Replace the CPU-rasterised painter's-algorithm renderer with a real GPU pipeline
(vertex+fragment shaders, depth buffer), demonstrating a plugin using raw SDL_GPU through Core's
exposed handles.

**Create** (`assets/shaders/src/` + committed `.spv`): `mesh.vert` / `mesh.frag` — MVP transform,
per-vertex / lit normals (Blinn-Phong in shader), matching a 3D vertex layout.

**Modify**:

- `source/GamePlugins/TeaPot3D/Render3DLayer/Render3DLayer.{h,cpp}` — build vertex / index buffers
  from `Mesh3D`; create an `SDL_GPUGraphicsPipeline` (depth-test enabled) + a depth texture sized to
  the viewport; in `Draw()`, record a render pass into `SDLLayer`'s command buffer with the 3D
  viewport / scissor, bind the mesh pipeline, upload uniforms (model / view / proj, light), and draw
  indexed. Remove `SDL_RenderGeometry` / `SDL_SetRenderViewport` / painter's sort.
- Keep the existing Lua API (`Render3D.*`) intact — only the rendering internals change.

**Constraints**:

- Plugin obtains `SDL_GPUDevice*` / command buffer / swapchain via `SDLLayer` accessors
  (design-principle: plugins get raw SDL).
- Depth buffer owned by the plugin; recreated on viewport resize.

**Acceptance**: Teapot renders with correct depth ordering and lighting; viewport placement matches
prior behavior; runs on top of 2D, below RmlUi.

**Status — DONE.** `Render3DLayer` ported to a real SDL_GPU pipeline: `mesh.vert`/`mesh.frag`
shaders, interleaved `MeshVertex` (pos/normal/diffuse/ambient) vertex + index buffers uploaded from
`Mesh3D` via a one-shot copy pass, `SDL_GPUGraphicsPipeline` with depth-test/write (`COMPAREOP_LESS`,
`CULLMODE_NONE`) + plugin-owned depth texture recreated on swapchain resize, and a per-frame render
pass (`perspectiveRH_ZO` + Y-flip, world-space diffuse+ambient lighting) recorded into `SDLLayer`'s
command buffer with viewport/scissor. All `SDL_Renderer`/`SDL_RenderGeometry`/painter's-sort code
removed; Lua `Render3D.*` API unchanged. TeaPot3D + `untitled.exe` build GREEN.

Two latent runtime issues were fixed while bringing the (now-runnable) app up:
- `SDLLayer` requested SPIRV|DXIL|MSL at device creation, so SDL selected the D3D12 backend
  (DXBC/DXIL only) while all shaders ship as SPIR-V → `SDL_CreateGPUShader` failed on the first
  sprite shader. Now requests `SDL_GPU_SHADERFORMAT_SPIRV` only, selecting the Vulkan backend.
- `~UGEApplication` destroyed `m_layers` front-to-back, freeing SDLLayer's GPU device before
  higher-order GPU-owning layers (RmlUILayer, Render3DLayer) released their resources → use-after-free
  at shutdown. Now tears layers down in reverse (LIFO) so SDLLayer (lowest load order) dies last.
App runs and shuts down cleanly on Vulkan.

---

## P6 — Plumbing cleanup + docs · build: GREEN (final)

**Goal**: Remove dead `SDL_Renderer` references and update documentation.

**Modify**:

- Delete `SDLRendererDeleter` / `UniqueRenderer` and any lingering `SDL_Renderer*` params across
  headers / comments.
- Docs: `docs/systems/SpriteSystem.md`, `docs/systems/Rendering3D.md`,
  `docs/architecture/FrameLoop.md`, `docs/reference/BuildWorkflow.md`, and the `AGENTS.md` layer
  table (SDLLayer role → "GPU device, swapchain, Renderer2D").

**Acceptance**: Full build green; no `SDL_Renderer` / `SDL_Texture` / `SDL_RenderTexture` references
remain in `source/`; docs describe the SDL_GPU pipeline.

**Status — DONE.** Removed the unused `SDLTextureDeleter` / `UniqueTexture` aliases from
`SDLLayer.h` (`SDLRendererDeleter` / `UniqueRenderer` were already gone since P1). Scrubbed the last
stale `SDL_Texture` / `SDL_RenderTexture` / `SDL_Renderer` mentions from header comments
(`GpuTexture.h`, `SpriteSheet.h`, `RmlUILayer.h`, `SpriteSheet` draw example). A repo-wide grep of
`source/` now returns **zero** `SDL_Renderer` / `SDL_Texture` / `SDL_RenderTexture` /
`SDL_RenderGeometry` / `SDL_SetRenderViewport` references. Docs updated to describe the SDL_GPU
pipeline: `SpriteSystem.md` (Sprites own `GpuTexture`, draw via `Renderer2D`; corrected `Load`/`Draw`
signatures), `Rendering3D.md` (GPU pipeline + depth buffer, fetches `SDL_GPUDevice`/command buffer),
`Animation.md` (`Renderer2D*` ctor), `FrameLoop.md` (BeginFrame acquires GPU command buffer +
swapchain, EndFrame submits), `BuildWorkflow.md` (`RmlUi_Renderer_SDL_GPU.cpp`), the `AGENTS.md`
layer table (SDLLayer → "SDL_GPU device + swapchain, Renderer2D"), and a caveat note on the
deferred `testing/HeadlessExecution.md` design record. Full build GREEN.

**Migration complete** — all phases P0–P6 done; the engine renders entirely through SDL_GPU.

---

## Open Items / Deferred

- **Shader build automation** (CMake custom command or CI) — deferred per decision; manual
  precompile documented in `assets/shaders/README.md`.
- **DXIL / MSL shader variants** — SPIR-V primary now; add cross-compiled variants when targeting
  D3D12 / Metal.
- **MSAA / advanced RmlUi effects** — validate filters / masks post-migration.

