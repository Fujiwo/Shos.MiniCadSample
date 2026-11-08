#pragma once

#include <afx.h>
#include "Geometry.h"

namespace Shos::MiniCadSample::Views {

using namespace Utilities;

class Zooming
{
    const CSize minimumSize;
    const CRect maximumArea;
    CRect       logicalArea;
    CWnd&       window;

    bool        isDragging;
    CPoint      dragStartPoint;
    CRect       dragStartLogicalArea;

public:
    Zooming(CWnd& window, const CSize& minimumSize, const CRect& maximumArea)
        : minimumSize(minimumSize), maximumArea(maximumArea), logicalArea(maximumArea), window(window), isDragging(false)
    {
        ASSERT_VALID(&window);
    }

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

        const auto deltaValue     = delta / (double)WHEEL_DELTA;
        const auto denominator    = 10.0;
        auto            newLogicalArea = logicalArea;

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
    bool ShiftLogicalArea(const CRect& area)
    {
        auto oldLogicalArea = logicalArea;
        logicalArea              = Geometry::ShiftInto(area, maximumArea);

        if (logicalArea != oldLogicalArea)
            TRACE(_T("Zooming::SetLogicalArea(left: %d, top: %d, width: %d, height: %d)\n"), logicalArea.left, logicalArea.top, logicalArea.Width(), logicalArea.Height());

        return logicalArea != oldLogicalArea;
    }

    bool SetLogicalArea(const CRect& area)
    {
        auto oldLogicalArea = logicalArea;

        auto newArea = Geometry::EnlargeTo(area, minimumSize);
        logicalArea = Geometry::ShiftInto(newArea, maximumArea);
        logicalArea.IntersectRect(logicalArea, maximumArea);

        ASSERT(logicalArea.Width() > 0 && logicalArea.Height() > 0);
        return logicalArea != oldLogicalArea;
    }

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
