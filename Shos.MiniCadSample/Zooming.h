#pragma once

// Zooming and panning of a window: keeps the visible part of the drawing area in logical coordinates
// and maps it to the client area.

#include <afx.h>
#include "Geometry.h"

namespace Shos::MiniCadSample::Views {

using namespace Utilities;

// The visible part of the drawing area (logicalArea) is always inside maximumArea
// and at least minimumSize. It is mapped to the whole client area, keeping the aspect ratio (MM_ISOTROPIC).
// Changing methods return true if the visible area changed, so that the caller knows it must redraw.
class Zooming
{
    const CSize minimumSize;     // The smallest visible size (the largest zoom-in).
    const CRect maximumArea;     // The whole drawing area (the largest zoom-out).
    CRect       logicalArea;     // The visible part, in logical coordinates.
    CWnd&       window;

    bool        isDragging = false;
    CPoint      dragStartPoint;        // Where the drag started, in logical coordinates at that time.
    CRect       dragStartLogicalArea;  // The visible part when the drag started.

public:
    // At first the whole maximumArea is visible.
    Zooming(CWnd& window, const CSize& minimumSize, const CRect& maximumArea)
        : minimumSize(minimumSize), maximumArea(maximumArea), logicalArea(maximumArea), window(window)
    {
        ASSERT_VALID(&window);
    }

    // Sets the mapping for the current visible area.
    void PrepareDC(CDC& dc)
    {
        PrepareDC(dc, logicalArea);
    }

    // Maps area (logical coordinates) to the client area.
    void PrepareDC(CDC& dc, const CRect& area)
    {
        dc.SetMapMode(MM_ISOTROPIC);

        dc.SetWindowOrg(area.CenterPoint());
        dc.SetWindowExt(area.Size());

        CRect clientRect;
        window.GetClientRect(clientRect);
        dc.SetViewportOrg(clientRect.CenterPoint());
        dc.SetViewportExt(clientRect.Size());
    }

    // point: the position of the mouse in screen coordinates (as WM_MOUSEWHEEL gives it).
    // Zooms with the point under the mouse as the center.
    bool OnMouseWheel(UINT keys, short delta, CPoint point)
    {
        if ((keys & MK_CONTROL) == 0)
            return false;

        window.ScreenToClient(&point);
        point = DPtoLP(point);

        // One wheel notch (WHEEL_DELTA) changes the size of the visible area by 10 percent:
        // zoom in for a positive delta (wheel forward), zoom out for a negative one.
        const auto     deltaValue     = delta / static_cast<double>(WHEEL_DELTA);
        constexpr auto denominator    = 10.0;
        auto           newLogicalArea = logicalArea;

        Geometry::Enlarge(newLogicalArea, point, (denominator - deltaValue) / denominator);
        return SetLogicalArea(newLogicalArea);
    }

    // Panning by dragging. The points are in logical coordinates, converted with the mapping at the time of each event.
    void OnDragStart(CPoint point)
    {
        dragStartPoint       = point;
        dragStartLogicalArea = logicalArea;
        isDragging           = true;
    }

    // Follows the mouse while dragging. Returns whether the view moved.
    bool OnDragging(CPoint point)
    {
        return ShiftTo(point);
    }

    // Stops panning where the view is (it is not moved back).
    void OnDraggingAbort()
    {
        isDragging = false;
    }

    // Moves the view to the final position and ends the panning. Returns whether the view moved.
    bool OnDragEnd(CPoint point)
    {
        const auto shifted = ShiftTo(point);
        isDragging = false;
        return shifted;
    }

    // Moves the view so that the point where the drag started comes under the mouse.
    bool ShiftTo(CPoint point)
    {
        if (!isDragging)
            return false;

        // point was converted with the current mapping, which has already moved during the drag.
        // Converts it back to the device (client) coordinates, and then to the logical coordinates at the start of the drag,
        // where dragStartPoint is.
        const auto devicePoint      = LPtoDP(point, logicalArea);
        const auto pointAtDragStart = DPtoLP(devicePoint, dragStartLogicalArea);
        return ShiftLogicalArea(dragStartLogicalArea + (dragStartPoint - pointAtDragStart));
    }

private:
    // Moves the visible area (without changing its size), keeping it inside maximumArea.
    bool ShiftLogicalArea(const CRect& area)
    {
        const auto oldLogicalArea = logicalArea;
        logicalArea               = Geometry::ShiftInto(area, maximumArea);

        if (logicalArea != oldLogicalArea)
            TRACE(_T("Zooming::SetLogicalArea(left: %d, top: %d, width: %d, height: %d)\n"), logicalArea.left, logicalArea.top, logicalArea.Width(), logicalArea.Height());

        return logicalArea != oldLogicalArea;
    }

    // Sets the visible area, enlarged to at least minimumSize, and moved/clipped into maximumArea.
    bool SetLogicalArea(const CRect& area)
    {
        auto oldLogicalArea = logicalArea;

        auto newArea = Geometry::EnlargeTo(area, minimumSize);
        logicalArea = Geometry::ShiftInto(newArea, maximumArea);
        logicalArea.IntersectRect(logicalArea, maximumArea);

        ASSERT(logicalArea.Width() > 0 && logicalArea.Height() > 0);
        return logicalArea != oldLogicalArea;
    }

    // Device (client) to logical coordinates with the current visible area.
    CPoint DPtoLP(CPoint point)
    {
        return DPtoLP(point, logicalArea);
    }

    // Converts point with the mapping that shows area.
    CPoint DPtoLP(CPoint point, const CRect& area)
    {
        ASSERT(window.GetSafeHwnd() != nullptr);

        CClientDC dc(&window);
        PrepareDC(dc, area);
        dc.DPtoLP(&point);
        return point;
    }

    // The reverse of DPtoLP(point, area).
    CPoint LPtoDP(CPoint point, const CRect& area)
    {
        ASSERT(window.GetSafeHwnd() != nullptr);

        CClientDC dc(&window);
        PrepareDC(dc, area);
        dc.LPtoDP(&point);
        return point;
    }
};

} // namespace Shos::MiniCadSample::Views
