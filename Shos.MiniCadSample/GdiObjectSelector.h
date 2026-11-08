#pragma once

// RAII helpers that select a GDI object into a device context and restore the previous one at scope exit.

#include <afx.h>

namespace Shos {

// Common part: restores the object that was selected before. Not copyable.
// The selected object must outlive the selector's scope (it is deselected, not deleted, here).
class GdiObjectSelectorBase
{
    CDC&              dc;
    CGdiObject* const oldGdiObject;

public:
    GdiObjectSelectorBase(CDC& dc, CGdiObject* oldGdiObject) : dc(dc), oldGdiObject(oldGdiObject)
    {}

    virtual ~GdiObjectSelectorBase()
    {
        dc.SelectObject(oldGdiObject);
    }

    GdiObjectSelectorBase(const GdiObjectSelectorBase&) = delete;
    GdiObjectSelectorBase& operator=(const GdiObjectSelectorBase&) = delete;
};

// Selects a stock object (e.g. NULL_BRUSH, the index of GetStockObject) into dc.
class StockObjectSelector : public GdiObjectSelectorBase
{
public:
    StockObjectSelector(CDC& dc, int stockObjectIndex) : GdiObjectSelectorBase(dc, dc.SelectStockObject(stockObjectIndex))
    {}
};

// Selects gdiObject (a pen, brush, font and so on) into dc.
class GdiObjectSelector : public GdiObjectSelectorBase
{
public:
    GdiObjectSelector(CDC& dc, CGdiObject& gdiObject) : GdiObjectSelectorBase(dc, dc.SelectObject(&gdiObject))
    {}
};

} // namespace Shos
