# Asset System

Assets are loaded exclusively through the **PhysicsFS virtual filesystem (VFS)**, wrapped by
`PhysFSLayer`. All file I/O — including RmlUi `.rml`/`.rcss`/fonts — routes through this layer.

Related: [Configuration.md](../reference/Configuration.md) ·
[DataStore.md](DataStore.md) · [SpriteSystem.md](SpriteSystem.md)
Key file: [`PhysFSLayer.h`](../../source/UGECore/Public/Layers/PhysFSLayer.h)

---

## Layout & Packing

- Assets live in `assets/` at the source root.
- On each build, `pack_assets.cmake` packs `assets/` into `cmake-build-debug/assets.pak`.
- **`OVERRIDE/assets`** (source root) is copied via `cmake -E copy_directory_if_different`
  to `cmake-build-debug/OVERRIDE/assets/` on every build — commit source-tracked overrides here.

---

## Runtime Mounting

At runtime:

- `assets.pak` is mounted to VFS root `/` with **prepend=true**.
- `OVERRIDE/assets/` (inside the build output dir) is also mounted to `/assets` with
  **prepend=true** (higher priority, last-mounted wins) — drop files there to override without
  rebuilding the pak.
- If a mount path does not exist on disk, `PhysFSLayer` logs a warning and **skips** it
  (non-fatal).

Mount points are declared in `UGESettings.toml` — see
[Configuration.md](../reference/Configuration.md).

---

## Virtual Path Convention

Virtual paths always start with `assets/…` (**no leading slash**), e.g.
`"assets/ui/main.rml"`.

RmlUi file access is bridged through `PhysFSFileInterface` (implements `Rml::FileInterface`).

---

## PhysFSLayer Helpers

Beyond `Mount()` / `Unmount()`:

| Method | Returns | Purpose |
|---|---|---|
| `ReadFile(path)` | `std::vector<std::byte>` | Read a whole file. |
| `OpenAsIOStream(path)` | `SDL_IOStream*` | Open as an SDL stream. |
| `Exists(path)` | `bool` | Test existence. |
| `ListFiles(dir)` | `std::vector<std::string>` | Enumerate a directory. |
| `LogFiles(dir = "")` | — | Log all VFS files to `LoggingLayer` (called in `Create()` after mounting). |

Free helper `LoadTextureFromPhysFS(VirtualPath)` → `std::expected<UniqueTexture, std::string>`
(declared in `Layers/SDLLayer.h`, resolves renderer + PhysFS via `ServiceLocator`) loads an
image from the VFS into an SDL texture.

