#pragma once

#include <afx.h>
#include <limits>
#include <random>
#include "FigureAttribute.h"
#include "GdiObjectSelector.h"
#include "Geometry.h"

class Figure : public CObject
{
	static const long	  selectorSize	   = 10L;
	static const long	  selectorPenWidth = 5;
	static const COLORREF selectedColor	   = RGB(0x80, 0x00, 0x40);
	static const COLORREF areaColor		   = RGB(0x00, 0xa0, 0xff);
	
	FigureAttribute attribute;
	bool            isSelected;

public:
	const FigureAttribute& Attribute() const
	{
		return attribute;
	}

	FigureAttribute& Attribute()
	{
		return attribute;
	}

	Figure() : isSelected(false)
	{}

	Figure(const Figure& another) : attribute(another.attribute), isSelected(another.isSelected)
	{}

	Figure& operator =(const Figure& another)
	{
		attribute  = another.attribute;
		isSelected = another.isSelected;
		return *this;
	}

	bool IsSelected() const
	{
		return isSelected;
	}

	void Select(bool selected)
	{
		this->isSelected = selected;
	}

	virtual Figure* Clone()
	{
		return new Figure(*this);
	}

	void Draw(CDC& dc) const
	{
		StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
		CPen				pen(PS_SOLID, attribute.GetPenWidth(), attribute.GetColor());
		GdiObjectSelector   penSelector(dc, pen);

		DrawShape(dc);
		if (isSelected)
			DrawSelecter(dc);
	}

	void DrawArea(CDC& dc) const
	{
		StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
		CPen				pen(PS_SOLID, 3, areaColor);
		GdiObjectSelector	penSelector(dc, pen);

		dc.Rectangle(GetArea());
	}

	CRect GetArea() const
	{
		auto area   = GetShapeArea();
		auto  margin = attribute.GetPenWidth() + selectorSize + selectorPenWidth;
		area.InflateRect(margin, margin);
		return area;
	}

	virtual long GetDistanceFrom(CPoint point) const;
		
	virtual void Serialize(CArchive& ar) override
	{
		CObject::Serialize(ar);
		attribute.Serialize(ar);
	}

protected:
	virtual void DrawShape(CDC& dc) const
	{}

	virtual CRect GetShapeArea() const
	{
		return CRect();
	}

	virtual std::vector<CPoint> GetPoints() const
	{
		return std::vector<CPoint>();
	}
	
private:
	void DrawSelecter(CDC& dc) const
	{
		StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
		CPen				pen(PS_SOLID, selectorPenWidth, selectedColor);
		GdiObjectSelector	penSelector(dc, pen);

		auto points = GetPoints();
		std::for_each(points.begin(), points.end(), [&](const CPoint& point) { DrawSelecter(dc, point); });
	}

	void DrawSelecter(CDC& dc, CPoint point) const
	{
		CRect rect(point, point);
		rect.InflateRect(selectorSize, selectorSize);
		dc.Rectangle(rect);
	}

	DECLARE_SERIAL(Figure)
};

class DotFigure : public Figure
{
	const long radius = 10L;
	CPoint     position;
		
public:
	DotFigure()
	{}

	DotFigure(const DotFigure& another) : Figure(another), position(another.position)
	{}

	DotFigure(const CPoint& position) : position(position)
	{}

	DotFigure& operator =(const DotFigure& another)
	{
		Figure::operator =(another);
		position = another.position;
		return *this;
	}

	virtual Figure* Clone() override
	{
		return new DotFigure(*this);
	}

	virtual long GetDistanceFrom(CPoint point) const override
	{
		auto distance = Geometry::GetDistance(point, position) - radius;
		return distance > 0L ? distance : 0L;
	}

	virtual void Serialize(CArchive& ar) override
	{
		Figure::Serialize(ar);

		if (ar.IsStoring())
			ar << position;
		else
			ar >> position;
	}

protected:
	virtual void DrawShape(CDC& dc) const override
	{
		dc.Ellipse(GetShapeArea());
	}

	virtual CRect GetShapeArea() const override
	{
		const CSize size(radius, radius);
		return CRect(position - size, position + size);
	}

	virtual std::vector<CPoint> GetPoints() const
	{
		return { position };
	}

	DECLARE_SERIAL(DotFigure)
};

class LineFigure : public Figure
{
	CPoint start, end;

public:
	LineFigure()
	{}

	LineFigure(const LineFigure& another) : Figure(another), start(another.start), end(another.end)
	{}

	LineFigure(CPoint start, CPoint end) : start(start), end(end)
	{}

	LineFigure& operator =(const LineFigure& another)
	{
		Figure::operator =(another);
		start = another.start;
		end = another.end;
		return *this;
	}

	virtual Figure* Clone() override
	{
		return new LineFigure(*this);
	}

	virtual long GetDistanceFrom(CPoint point) const override
	{
		return Geometry::GetDistanceToLineSegment(point, start, end);
	}

	virtual void Serialize(CArchive& ar) override
	{
		Figure::Serialize(ar);

		if (ar.IsStoring())
			ar << start << end;
		else
			ar >> start >> end;
	}

protected:
	virtual void DrawShape(CDC& dc) const override
	{
		dc.MoveTo(start);
		dc.LineTo(end);
	}

	virtual CRect GetShapeArea() const override
	{
		CRect area(start, end);
		area.NormalizeRect();
		return area;
	}

	virtual std::vector<CPoint> GetPoints() const
	{
		return { start, end };
	}

	DECLARE_SERIAL(LineFigure)
};

class RectangleFigureBase : public Figure
{
protected:
	CRect position;

public:
	RectangleFigureBase()
	{}

	RectangleFigureBase(const RectangleFigureBase& another) : Figure(another), position(another.position)
	{}

	RectangleFigureBase(const CRect& position) : position(position)
	{
		this->position.NormalizeRect();
	}

	RectangleFigureBase& operator =(const RectangleFigureBase& another)
	{
		Figure::operator =(another);
		position = another.position;
		return *this;
	}

	virtual void Serialize(CArchive& ar) override
	{
		Figure::Serialize(ar);

		if (ar.IsStoring())
			ar << position;
		else
			ar >> position;
	}

protected:
	virtual CRect GetShapeArea() const override
	{
		return position;
	}

	virtual std::vector<CPoint> GetPoints() const
	{
		return Geometry::ToPoints(position);
	}

	DECLARE_SERIAL(RectangleFigureBase)
};

class RectangleFigure : public RectangleFigureBase
{
public:
	RectangleFigure()
	{}

	RectangleFigure(const CRect& position) : RectangleFigureBase(position)
	{}

	RectangleFigure& operator =(const RectangleFigure& another)
	{
		RectangleFigureBase::operator =(another);
		return *this;
	}

	virtual Figure* Clone() override
	{
		return new RectangleFigure(*this);
	}

protected:
	virtual void DrawShape(CDC& dc) const override
	{
		dc.Rectangle(&position);
	}

	virtual long GetDistanceFrom(CPoint point) const override
	{
		return Geometry::GetDistance(point, position);
	}

	DECLARE_SERIAL(RectangleFigure)
};

class EllipseFigure : public RectangleFigureBase
{
public:
	EllipseFigure()
	{}

	EllipseFigure(const CRect& position) : RectangleFigureBase(position)
	{}

	EllipseFigure& operator =(const EllipseFigure& another)
	{
		RectangleFigureBase::operator =(another);
		return *this;
	}

	virtual Figure* Clone() override
	{
		return new EllipseFigure(*this);
	}

protected:
	virtual void DrawShape(CDC& dc) const override
	{
		dc.Ellipse(&position);
	}

	virtual long GetDistanceFrom(CPoint point) const override
	{
		return Geometry::GetDistanceToEllipse(point, position);
	}

	DECLARE_SERIAL(EllipseFigure)
};

class FigureHelper
{
	static const long		  figureKindNumber = 4;

	static std::random_device random;
	static std::mt19937		  mt;

public:
	static std::vector<Figure*> GetRandomFigures(size_t count, const CRect& area)
	{
		std::vector<Figure*> figures;
		for (size_t counter = 0; counter < count; counter++)
			figures.push_back(GetRandomFigure(area));
		return figures;
	}

	static bool GetArea(std::vector<Figure*> figures, CRect& area)
	{
		if (figures.size() == 0)
			return false;

		std::vector<CRect> areas(figures.size());
		std::transform(figures.begin(), figures.end(), areas.begin(), [](Figure* figure) { return figure->GetArea(); });
		return Geometry::GetArea(areas, area);
	}

private:
	static Figure* GetRandomFigure(const CRect& area)
	{
		Figure* figure = nullptr;
		
		switch (RandomValue(0, figureKindNumber - 1)) {
		case 0:
			figure = new DotFigure(RandomPosition(area));
			break;
		case 1:
			figure = new LineFigure(RandomPosition(area), RandomPosition(area));
			break;
		case 2:
			figure = new RectangleFigure(CRect(RandomPosition(area), RandomPosition(area)));
			break;
		case 3:
			figure = new EllipseFigure(CRect(RandomPosition(area), RandomPosition(area)));
			break;
		default:
			ASSERT(false);
			figure = nullptr;
			break;
		}
		figure->Attribute().SetColor   (RandomColor()						  );
		figure->Attribute().SetPenWidth(RandomValue(0, 5));
		return figure;
	}

	static CPoint RandomPosition(const CRect& area)
	{
		return CPoint(RandomValue(area.left, area.right), RandomValue(area.top, area.bottom));
	}

	static LONG RandomValue(long minimum, long maxmum)
	{
		return minimum + mt() % (maxmum - minimum + 1);
	};

	static COLORREF RandomColor()
	{
		return RGB(RandomValue(0, 255), RandomValue(0, 255), RandomValue(0, 255));
	}
};
