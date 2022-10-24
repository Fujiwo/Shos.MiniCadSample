#pragma once

#include <afx.h>
#include "Geometry.h"

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
        : window(window), minimumSize(minimumSize), maximumArea(maximumArea), logicalArea(maximumArea), isDragging(false)
    {
        ASSERT_VALID(&window);
    }

    void PrepareDC(CDC& dc)
    {
        dc.SetMapMode(MM_ISOTROPIC);

        dc.SetWindowOrg(logicalArea.CenterPoint());
        dc.SetWindowExt(logicalArea.Size());

        CRect clientRect;
        window.GetClientRect(clientRect);
        dc.SetViewportOrg(clientRect.CenterPoint());
        dc.SetViewportExt(clientRect.Size());
    }

    bool OnMouseWheel(UINT keys, short delta, CPoint point)
    {
        if ((keys & MK_CONTROL) == 0)
            return false;

        DPtoLP(point);

        const auto deltaValue     = delta / (double)WHEEL_DELTA;
        const auto denominator    = 10.0;
        auto            newLogicalArea = logicalArea;

        Geometry::Enlarge(newLogicalArea, point, (denominator - deltaValue) / denominator);
        return SetLogicalArea(newLogicalArea);
    }

    void OnDragStart(CPoint point)
    {
        dragStartPoint       = point;
        dragStartLogicalArea = logicalArea;
        isDragging           = true;
    }

    //bool OnDragging(CPoint point)
    //{
    //    //return ShiftTo(point);
    //}

    void OnDraggingAbort()
    {
        isDragging = false;
    }

    bool OnDragEnd(CPoint point)
    {
        return ShiftTo(point);
    }

    bool ShiftTo(CPoint point)
    {
        auto newLogicalArea = dragStartLogicalArea + (dragStartPoint - point);
        return isDragging ? ShiftLogicalArea(dragStartLogicalArea + (dragStartPoint - point)) : false;
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
        ASSERT(window.GetSafeHwnd() != nullptr);

        CClientDC dc(&window);
        PrepareDC(dc);
        dc.DPtoLP(&point);
        return point;
    }
};
