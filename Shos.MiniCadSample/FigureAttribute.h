#pragma once

#include <afx.h>
#include "Observer.h"

class FigureAttribute //: public Observable<FigureAttribute>
{
	COLORREF color;
	int      penWidth;

public:
	COLORREF GetColor() const
	{
		return color;
	}

	bool SetColor(COLORREF color)
	{
		if (color != this->color) {
			this->color = color;
			//NotifyObservers(*this);
			return true;
		}
		return false;
	}

	int GetPenWidth() const
	{
		return penWidth;
	}

	bool SetPenWidth(int penWidth)
	{
		if (penWidth != this->penWidth) {
			this->penWidth = penWidth;
			//NotifyObservers(*this);
			return true;
		}
		return false;
	}

	FigureAttribute() : color(RGB(0x00, 0x00, 0x00)), penWidth(0)
	{}

	void Serialize(CArchive& ar)
	{
		if (ar.IsStoring())
			ar << color << penWidth;
		else
			ar >> color >> penWidth;
	}
};
