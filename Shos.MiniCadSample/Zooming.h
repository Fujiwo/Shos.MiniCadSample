#pragma once

#include <afx.h>
#include "Geometry.h"

class Zooming
{
    const CSize minimumSize;
    const CRect maximumArea;
    CRect       logicalArea;
    CWnd&       window;

public:
    Zooming(CWnd& window, const CSize& minimumSize, const CRect& maximumArea)
        : window(window), minimumSize(minimumSize), maximumArea(maximumArea), logicalArea(maximumArea)
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

        const auto deltaValue = delta / (double)WHEEL_DELTA;
        const auto denominator = 10.0;
        auto            newLogicalArea = logicalArea;

        Geometry::Enlarge(newLogicalArea, point, (denominator - deltaValue) / denominator);
        SetLogicalArea(newLogicalArea);

        return true;
    }

private:
    void SetLogicalArea(const CRect& area)
    {
        logicalArea = area;
        logicalArea.IntersectRect(logicalArea, maximumArea);
        logicalArea = EnlargeTo(logicalArea, minimumSize);
    }

    static CRect EnlargeTo(const CRect& rect, CSize size)
    {
        auto d = max(size.cx - rect.Width(), size.cy - rect.Height());
        if (d <= 0)
            return rect;

        CRect newRect = rect;
        newRect.InflateRect(d, d);
        return newRect;
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
