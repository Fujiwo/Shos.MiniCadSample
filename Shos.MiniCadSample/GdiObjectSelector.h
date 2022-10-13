#pragma once

#include <afx.h>
#include <random>

class GdiObjectSelectorBase
{
	CDC&			  dc;
	CGdiObject* const oldGdiObject;

public:
	GdiObjectSelectorBase(CDC& dc, CGdiObject* oldGdiObject) : dc(dc), oldGdiObject(oldGdiObject)
	{}

	virtual ~GdiObjectSelectorBase()
	{
		dc.SelectObject(oldGdiObject);
	}
};

class StockObjectSelector : public GdiObjectSelectorBase
{
public:
	StockObjectSelector(CDC& dc, int stockObjectIndex) : GdiObjectSelectorBase(dc, dc.SelectStockObject(stockObjectIndex))
	{}
};

class GdiObjectSelector : public GdiObjectSelectorBase
{
public:
	GdiObjectSelector(CDC& dc, CGdiObject& gdiObject) : GdiObjectSelectorBase(dc, dc.SelectObject(&gdiObject))
	{}
};
