#pragma once
#include <GameClasses/BoxCollision.h>
#include <vector>

// ── BoxCollisionGrid ──────────────────────────────────────────────────────────
// Broad-phase uniform spatial grid for AABB collision queries.
//
// Partitions a set of BoxCollision pointers into a 2-D grid of cells, each
// sized Coarseness × Coarseness units in reference space.  The grid is STATIC
// after construction — box positions must not change, but the Enabled flag may
// be toggled freely at any time.
//
// Construction:
//   std::vector<BoxCollision*> tiles = { ... };
//   BoxCollisionGrid grid(&tiles, 160);
//
// Query:
//   std::vector<BoxCollision*> hits;
//   if (grid.Intersects(&playerBox, hits)) { /* narrow-phase against hits */ }
//
// The grid stores non-owning pointers.  The caller is responsible for ensuring
// the source vector and the BoxCollision objects outlive the grid.
class BoxCollisionGrid
{
public:
    // Default constructor — produces an empty grid; Intersects() always returns false.
    BoxCollisionGrid() = default;

    // Build the grid from a vector of BoxCollision pointers.
    // Null entries in the vector are silently skipped.
    // Coarseness is the cell side-length in reference-space units.
    explicit BoxCollisionGrid(const std::vector<BoxCollision*>* Boxes, int Coarseness = 100);

    // Find all enabled boxes in the grid that intersect Target (Target itself is
    // excluded from results).  Appends to OutIntersecting — does not clear it first.
    // Returns true if at least one intersection was found.
    // Returns false immediately if Target is null or disabled.
    bool Intersects(BoxCollision* Target, std::vector<BoxCollision*>& OutIntersecting) const;

    // ── Grid metadata ──────────────────────────────────────────────────────────
    [[nodiscard]] float MinX()       const { return m_minX; }
    [[nodiscard]] float MinY()       const { return m_minY; }
    [[nodiscard]] float MaxX()       const { return m_maxX; }
    [[nodiscard]] float MaxY()       const { return m_maxY; }
    [[nodiscard]] int   Cols()       const { return m_cols; }
    [[nodiscard]] int   Rows()       const { return m_rows; }
    [[nodiscard]] int   Coarseness() const { return m_coarseness; }

private:
    // Snap helpers — operate on integer reference-space coordinates.
    static int floorMultiple(int Value, int Course);
    static int ceilMultiple (int Value, int Course);

    // Fill (ColMin, ColMax, RowMin, RowMax) with the cell range overlapped by Box.
    // All values are clamped to valid grid indices.
    void cellRangeFor(const BoxCollision* Box,
                      int& ColMin, int& ColMax,
                      int& RowMin, int& RowMax) const;

    float m_minX = 0.0f, m_minY = 0.0f;   // grid origin in reference space
    float m_maxX = 0.0f, m_maxY = 0.0f;   // grid far corner
    int   m_cols = 0,    m_rows = 0;
    int   m_coarseness = 100;

    // Flat row-major grid: m_cells[row * m_cols + col]
    std::vector<std::vector<BoxCollision*>> m_cells;

    // Scratch buffer reused across Intersects() calls to deduplicate candidates
    // without a per-call heap allocation.  Mutable so Intersects() can be const.
    mutable std::vector<BoxCollision*> m_queryBuffer;
};
