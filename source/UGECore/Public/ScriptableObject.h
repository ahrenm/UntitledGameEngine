#pragma once
// ── Backward-compatibility shim ───────────────────────────────────────────────
// ScriptableObject has been renamed to IScriptableObject.
// Include <IScriptableObject.h> in new code.
#include <IScriptableObject.h>
using ScriptableObject = IScriptableObject;
