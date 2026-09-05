# Build & Workflow

**IDE**: CLion on Windows. CMake is managed by CLion — reload the CMake project via
**File → Reload CMake Project** (or the CMake tool window) after editing `CMakeLists.txt` or any
`FetchContent` dependency. Do **not** run `cmake -B` manually unless working outside CLion, as
CLion manages its own build directory (`cmake-build-debug/`).

Related: [Configuration.md](Configuration.md) · [KeyFiles.md](KeyFiles.md)

---

## Build Commands

```powershell
# Configure (first time or after CMakeLists changes — prefer CLion's CMake reload instead)
cmake -B cmake-build-debug -S .

# Required bootstrap for agent/tool-driven terminal builds: CLion's cached GCC
# expects helper tools like as.exe to be discoverable on PATH.
$env:PATH = 'C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin;' + $env:PATH

# Preferred full build command for agent/tool-driven sessions on this workspace.
# Use the absolute build directory and explicit target/job count format.
cmake --build C:\Users\ahren\CLionProjects\untitled\cmake-build-debug --target all -j 62

# Shorter equivalent when already operating from the project root.
cmake --build cmake-build-debug

# Repack assets only, without recompiling
cmake --build cmake-build-debug --target pack_assets

# Run
./cmake-build-debug/untitled.exe
```

### Agent Terminal Note

When an AI agent needs to build from a terminal, **first** prepend
`C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin` to `PATH`, then prefer the exact
absolute-path build form above instead of composing a relative build command with shell piping.
Without the MinGW `bin` directory on `PATH`, CLion's bundled `g++.exe` may fail because helper
tools like `as.exe` are not discoverable in agent terminal sessions.

---

## Library Structure

The project builds a **`UGECore` shared library** (`libUGECore.dll`) consumed by `untitled.exe`.
RmlUi backend sources are compiled into `UGECore` (not directly into the executable):

- `_deps/rmlui-src/Backends/RmlUi_Platform_SDL.cpp`
- `_deps/rmlui-src/Backends/RmlUi_Renderer_SDL_GPU.cpp` (the SDL_GPU render backend)

The macro `RMLUI_SDL_VERSION_MAJOR=3` is defined on `UGECore` (see
`source/UGECore/CMakeLists.txt`). Public headers are under `source/UGECore/Public/`; use
`#include <Layers/RmlUILayer.h>` etc. (the `Public/` dir is the include root).

---

## Compile Definitions

- **`RMLUI_STATIC_LIB`**: UGECore defines this as a `PUBLIC` compile definition. Any target that
  includes UGECore's public headers (e.g. `DemoGame.dll`) automatically receives it — do **not**
  redefine it manually.
- **`RMLUI_SDL_VERSION_MAJOR=3`**: defined on `UGECore`.

---

## MinGW Symbol Export

UGECore is built with `-Wl,-u,_ZN3Rml10FamilyBase8GetNewIdEv` so that `Rml::FamilyBase::GetNewId`
(in `librmlui.a:Traits.cpp.obj`) is forced into the UGECore link and therefore exported via
MinGW's default export-all behaviour.

Without this, templates in RmlUi headers (e.g. `Rml::Family<T>::Id()` → `GetNewId()`) produce
`undefined reference` linker errors in `DemoGame.dll` even though the symbol physically lives
inside `UGECore.dll`.

**If a new `Rml::Family<T>::Id()` instantiation causes a similar error** for a different symbol
in `librmlui.a`, add another `-Wl,-u,<mangled_name>` to `target_link_options(UGECore ...)` in
`source/UGECore/CMakeLists.txt`.

