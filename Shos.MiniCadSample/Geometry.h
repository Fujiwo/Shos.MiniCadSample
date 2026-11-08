#pragma once

// Geometry: static helpers for distances, rectangles and logical/device coordinate conversion.
// Distances and coordinates are logical units unless a function says otherwise.

#include <afx.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>
#include <ranges>
#include <vector>

namespace Shos::MiniCadSample::Utilities {

// A static-only utility class.
class Geometry
{
public:
    // The distance that means "very far" (also the starting value when searching for a minimum).
    static constexpr long maximumDistance = (std::numeric_limits<long>::max)(); // (...) avoids the max macro of <windows.h>.

    template <typename T>
        requires std::integral<T> || std::floating_point<T>
    static constexpr T Square(T x)
    {
        return x * x;
    }
    
    // Rounds to the nearest integer (halves go up).
    static long Round(double x)
    {
        return static_cast<long>(std::floor(x + 0.5));
    }
    
    // The length of the vector from the origin to point (rounded).
    static long Abs(CPoint point)
    {
        return Round(std::sqrt(Square(static_cast<double>(point.x)) + Square(static_cast<double>(point.y))));
    }
    
    // The length of the vector size (rounded).
    static long Abs(CSize size)
    {
        return Round(std::sqrt(Square(static_cast<double>(size.cx)) + Square(static_cast<double>(size.cy))));
    }

    // The distance between two points.
    static long GetDistance(CPoint point1, CPoint point2)
    {
        return Abs(point2 - point1);
    }
    
    // The distance from point to the segment start-end (to the nearer end point if the foot of the
    // perpendicular is outside the segment). A zero-length segment is treated as a point.
    static long GetDistanceToLineSegment(CPoint point, CPoint start, CPoint end)
    {
        auto d  = end   - start;
        auto length = Abs(d);
        if (length == 0)
            return GetDistance(point, start);

        auto d1 = start - point;
        auto d2 = end   - point;

        // Signs of the projections of the end points on the segment: f0 > 0 means point lies before start,
        // f1 < 0 means it lies after end; otherwise the foot of the perpendicular is on the segment.
        auto f0 = Mul(d, d1);
        auto f1 = Mul(d, d2);

        return f0 > 0 ? Abs(d1)
            : (f1 < 0 ? Abs(d2) : std::abs(d.cy * d1.cx - d.cx * d1.cy) / length);
    }

    // The distance from point to the outline (the four edges) of rect, not to its inside.
    static long GetDistance(CPoint point, const CRect& rect)
    {
        auto points = ToPoints(rect);
        auto distance = maximumDistance;

        for (size_t index = 0; index < points.size(); index++) {
            auto eachDistance = GetDistanceToLineSegment(point, points[index], points[(index + 1) % points.size()]);
            if (eachDistance < distance)
                distance = eachDistance;
        }
        return distance;
    }

    // The approximate distance from point to the outline of the ellipse inscribed in rect.
    // The plane is scaled so that the ellipse becomes a circle, the distance to the circle is taken, and
    // it is scaled back. This is exact on the axes and approximate elsewhere.
    // A degenerate (zero width or height) ellipse is a line segment.
    static long GetDistanceToEllipse(CPoint point, const CRect& rect)
    {
        if (rect.Width() == 0 || rect.Height() == 0)
            return GetDistanceToLineSegment(point, rect.TopLeft(), rect.BottomRight());

        auto rate = rect.Height() / static_cast<double>(rect.Width());

        auto d = point - rect.CenterPoint();
        d.cx = Round(d.cx * rate);

        auto distance = std::abs(Abs(d) - rect.Height() / 2);
        distance = Round(distance / rate);

        return distance;
    }

    // The inner (dot) product.
    static long Mul(CPoint point1, CPoint point2)
    {
        return point1.x * point2.x + point1.y * point2.y;
    }

    static long Mul(CSize size1, CSize size2)
    {
        return size1.cx * size2.cx + size1.cy * size2.cy;
    }

    // The corners of rect in order: top left, top right, bottom right, bottom left.
    static std::vector<CPoint> ToPoints(const CRect& rect)
    {
        return { rect.TopLeft(), CPoint(rect.right, rect.top), rect.BottomRight(), CPoint(rect.left, rect.bottom) };
    }
    
    // Sets area to the union of the (normalized) rectangles. Returns false (area untouched) if there are none.
    static bool GetArea(const std::vector<CRect>& areas, CRect& area)
    {
        if (areas.empty())
            return false;

        area = areas.front();
        area.NormalizeRect();
        for (auto eachArea : areas | std::views::drop(1)) {
            eachArea.NormalizeRect();
            area.UnionRect(area, eachArea);
        }
        return true;
    }

    // The nearest point to point in area, where area includes left and top but not right and bottom (as PtInRect).
    // area must not be empty.
    static CPoint Clamp(CPoint point, const CRect& area)
    {
        ASSERT(area.Width() > 0 && area.Height() > 0);
        return CPoint((std::min)((std::max)(point.x, area.left), area.right  - 1),
                      (std::min)((std::max)(point.y, area.top ), area.bottom - 1));
    }

    // Whether inner is inside outer, including the edges of outer (unlike PtInRect, which excludes right and bottom),
    // so that a figure touching the edge of a selection rectangle is selected.
    static bool InRect(CRect outer, CRect inner)
    {
        outer.NormalizeRect();
        inner.NormalizeRect();
        return outer.left <= inner.left && inner.right  <= outer.right &&
               outer.top  <= inner.top  && inner.bottom <= outer.bottom;
    }

    // Inflates rect equally on all sides until it is at least size wide and high. Returns rect if it already is.
    static CRect EnlargeTo(const CRect& rect, CSize size)
    {
        auto d = (std::max)(size.cx - rect.Width(), size.cy - rect.Height());
        if (d <= 0)
            return rect;

        CRect newRect = rect;
        newRect.InflateRect(d, d);

        ASSERT(newRect.Width () >= size.cx);
        ASSERT(newRect.Height() >= size.cy);

        return newRect;
    }

    // Moves area (without resizing it) so that it lies inside maximumArea, as far as it fits.
    // The right and bottom edges are fixed first, then the left and top (so the latter win if area is too big).
    static CRect ShiftInto(const CRect& area, const CRect& maximumArea)
    {
        ASSERT(area.Width () > 0);
        ASSERT(area.Height() > 0);

        auto newArea = area;
        if (newArea.right > maximumArea.right)
            newArea -= CSize(newArea.right - maximumArea.right, 0);
        if (newArea.bottom > maximumArea.bottom)
            newArea -= CSize(0, newArea.bottom - maximumArea.bottom);
        if (newArea.left < maximumArea.left)
            newArea += CSize(maximumArea.left - newArea.left, 0);
        if (newArea.top < maximumArea.top)
            newArea += CSize(0, maximumArea.top - newArea.top);

        ASSERT(newArea.Width () == area.Width ());
        ASSERT(newArea.Height() == area.Height());
        ASSERT(newArea.left >= maximumArea.left);
        ASSERT(newArea.top  >= maximumArea.top );

        return newArea;
    }

    // Converts a point from device (client) to logical coordinates, using the view's current mapping
    // (zoom and scroll position, as set up by OnPrepareDC).
    static CPoint DPtoLP(CView& view, CPoint point)
    {
        CClientDC dc(&view);
        view.OnPrepareDC(&dc);
        dc.DPtoLP(&point);
        return point;
    }

    // Converts a rectangle from logical to device (client) coordinates, using the view's current mapping.
    static CRect LPtoDP(CView& view, CRect rect)
    {
        CClientDC dc(&view);
        view.OnPrepareDC(&dc);
        dc.LPtoDP(rect);
        return rect;
    }

    // Converts a length (not a position) between device and logical units with the mapping of dc.
    static int DPtoLP(CDC& dc, int size)
    {
        CRect rect(0, 0, size, size);
        dc.DPtoLP(rect);
        return rect.Width();
    }

    static int LPtoDP(CDC& dc, int size)
    {
        CRect rect(0, 0, size, size);
        dc.LPtoDP(rect);
        return rect.Width();
    }

    // Scales rect by rate around basePoint (basePoint stays fixed). Used for zooming.
    static void Enlarge(CRect& rect, CPoint basePoint, double rate)
    {
        Enlarge(rect.left  , basePoint.x, rate);
        Enlarge(rect.top   , basePoint.y, rate);
        Enlarge(rect.right , basePoint.x, rate);
        Enlarge(rect.bottom, basePoint.y, rate);
    }
    
private:
    // Scales the distance of value from base by rate.
    static void Enlarge(long& value, long base, double rate)
    {
        value = Round(static_cast<double>(base) + (value - base) * rate);
    }
};

} // namespace Shos::MiniCadSample::Utilities
