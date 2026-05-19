#include <GameClasses/BoxCollisionGrid.h>
#include <algorithm>
#include <cmath>
#include <limits>

// ── Snap helpers ──────────────────────────────────────────────────────────────
// Both use inclusive semantics:
//   floorMultiple(127, 100) →  100    floorMultiple(0, 100)   → 0 (not -100)
//   ceilMultiple (847,  50) →  850    ceilMultiple (800, 100) → 800

int BoxCollisionGrid::floorMultiple(int Value, int Course)
{
    int f = (Value / Course) * Course;
    if (f > Value) f -= Course;   // correct negative truncation-toward-zero
    return f;
}

int BoxCollisionGrid::ceilMultiple(int Value, int Course)
{
    int f = (Value / Course) * Course;
    if (f < Value) f += Course;
    return f;
}

// ── Constructor ───────────────────────────────────────────────────────────────

BoxCollisionGrid::BoxCollisionGrid(const std::vector<BoxCollision*>* Boxes, int Coarseness)
    : m_coarseness(Coarseness)
{
    if (!Boxes || Boxes->empty() || Coarseness <= 0) return;

    // Pass 1: determine the tight bounding box of all supplied boxes.
    float tightMinX =  std::numeric_limits<float>::max();
    float tightMinY =  std::numeric_limits<float>::max();
    float tightMaxX = -std::numeric_limits<float>::max();
    float tightMaxY = -std::numeric_limits<float>::max();

    for (const BoxCollision* Box : *Boxes)
    {
        if (!Box) continue;
        tightMinX = std::min(tightMinX, Box->Left());
        tightMinY = std::min(tightMinY, Box->Top());
        tightMaxX = std::max(tightMaxX, Box->Right());
        tightMaxY = std::max(tightMaxY, Box->Bottom());
    }

    if (tightMinX > tightMaxX) return;   // all entries were null

    // Snap outward to whole multiples of Coarseness so cell boundaries are clean.
    // floor toward negative infinity for the min edges, ceil for the max edges.
    m_minX = static_cast<float>(floorMultiple(static_cast<int>(std::floor(tightMinX)), Coarseness));
    m_minY = static_cast<float>(floorMultiple(static_cast<int>(std::floor(tightMinY)), Coarseness));
    m_maxX = static_cast<float>(ceilMultiple (static_cast<int>(std::ceil (tightMaxX)), Coarseness));
    m_maxY = static_cast<float>(ceilMultiple (static_cast<int>(std::ceil (tightMaxY)), Coarseness));

    // Integer arithmetic — exact because m_min/max are whole multiples of Coarseness.
    m_cols = (static_cast<int>(m_maxX) - static_cast<int>(m_minX)) / Coarseness;
    m_rows = (static_cast<int>(m_maxY) - static_cast<int>(m_minY)) / Coarseness;

    if (m_cols <= 0 || m_rows <= 0) return;

    m_cells.resize(static_cast<size_t>(m_cols) * m_rows);
    m_queryBuffer.reserve(64);   // amortise; grows automatically on unusually large queries

    // Pass 2: insert each box into every cell whose AABB it overlaps.
    for (BoxCollision* Box : *Boxes)
    {
        if (!Box) continue;

        int colMin, colMax, rowMin, rowMax;
        cellRangeFor(Box, colMin, colMax, rowMin, rowMax);

        for (int Row = rowMin; Row <= rowMax; ++Row)
            for (int Col = colMin; Col <= colMax; ++Col)
                m_cells[static_cast<size_t>(Row) * m_cols + Col].push_back(Box);
    }
}

// ── Query ─────────────────────────────────────────────────────────────────────

bool BoxCollisionGrid::Intersects(BoxCollision* Target, std::vector<BoxCollision*>& OutIntersecting) const
{
    if (!Target || !Target->Enabled || m_cells.empty()) return false;

    int colMin, colMax, rowMin, rowMax;
    cellRangeFor(Target, colMin, colMax, rowMin, rowMax);

    // m_queryBuffer is the deduplication set for this call.
    // Linear search over a flat pointer array beats an unordered_set for the
    // small candidate counts (typically < 32) of a per-entity broad-phase query.
    m_queryBuffer.clear();
    m_queryBuffer.push_back(Target);   // pre-insert so Target is never emitted
    bool found = false;

    for (int Row = rowMin; Row <= rowMax; ++Row)
    {
        for (int Col = colMin; Col <= colMax; ++Col)
        {
            for (BoxCollision* Candidate : m_cells[static_cast<size_t>(Row) * m_cols + Col])
            {
                // Skip if already seen from an overlapping cell.
                if (std::find(m_queryBuffer.begin(), m_queryBuffer.end(), Candidate)
                    != m_queryBuffer.end()) continue;

                m_queryBuffer.push_back(Candidate);

                if (!Candidate->Enabled) continue;

                if (Candidate->Intersects(*Target))
                {
                    OutIntersecting.push_back(Candidate);
                    found = true;
                }
            }
        }
    }

    return found;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void BoxCollisionGrid::cellRangeFor(const BoxCollision* Box,
                                     int& ColMin, int& ColMax,
                                     int& RowMin, int& RowMax) const
{
    auto fc = static_cast<float>(m_coarseness);

    // A box overlaps cell (col) when:  box.Left() < cellRight  AND  box.Right() > cellLeft
    //   cellLeft  = m_minX + col * fc
    //   cellRight = m_minX + (col+1) * fc
    //
    // Smallest col where cellRight > box.Left():
    //   col > (box.Left() - m_minX) / fc - 1  →  colMin = floor((box.Left() - m_minX) / fc)
    //
    // Largest col where cellLeft < box.Right():
    //   col < (box.Right() - m_minX) / fc  →  colMax = ceil((box.Right() - m_minX) / fc) - 1
    //
    // Same logic applies for rows.

    ColMin = static_cast<int>(std::floor((Box->Left()   - m_minX) / fc));
    ColMax = static_cast<int>(std::ceil ((Box->Right()  - m_minX) / fc)) - 1;
    RowMin = static_cast<int>(std::floor((Box->Top()    - m_minY) / fc));
    RowMax = static_cast<int>(std::ceil ((Box->Bottom() - m_minY) / fc)) - 1;

    ColMin = std::max(ColMin, 0);
    ColMax = std::min(ColMax, m_cols - 1);
    RowMin = std::max(RowMin, 0);
    RowMax = std::min(RowMax, m_rows - 1);
}
