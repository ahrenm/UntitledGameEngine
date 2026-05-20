#pragma once
#include "DataValue.h"
#include "DataMeta.h"

// ── DataEntry ─────────────────────────────────────────────────────────────────
// The single unit stored in a DataStore: a type-erased DataValue paired with the
// DataMeta describing its persistence policy and provenance.
struct DataEntry
{
    DataValue Value;
    DataMeta  Meta;
};
