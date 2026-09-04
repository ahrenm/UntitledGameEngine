# Coding Conventions

Naming rules for new code in this codebase.

| Category | Convention | Example |
|---|---|---|
| **Public functions** | PascalCase | `LoadDocument()`, `SetBackground()`, `RegisterAll()` |
| **Private functions** | camelCase | `extractDataModelNames()`, `processFragments()`, `lastError()` |
| **Private member variables** | camelCase with `m_` prefix | `m_window`, `m_logFn`, `m_pageViewModels` |
| **Function parameters** | camelCase | `void Log(std::string Msg)`, `bool Mount(const char* RealPath, ...)` |
| **Local variables** | camelCase | `auto Result = ...`, `for (auto& Layer : m_layers)` |
| **Macros** | ALL_CAPS with underscores | `REGISTER_VIEWMODEL(...)`, `RMLUI_SDL_VERSION_MAJOR` |
| **`constexpr` / `const` expressions** | ALL_CAPS with underscores | `RMLUI_SDL_VERSION_MAJOR` |

## Notes & Exceptions

- Public data members (e.g. `onLog` in `LoggingLayer`) are **excluded** from the PascalCase rule
  — they follow camelCase.
- Override methods that must match a third-party virtual interface (e.g. `Rml::FileInterface`:
  `Open`, `Close`, `Read`, `Seek`, `Tell`, `Length`) keep their inherited names.
- Private helper methods inside `.cpp` files (lambdas, free functions with internal linkage)
  follow the camelCase convention.

