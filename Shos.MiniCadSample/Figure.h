#pragma once

#include <afx.h>
#include <algorithm>
#include <random>
#include <vector>
#include "FigureAttribute.h"
#include "GdiObjectSelector.h"
#include "Geometry.h"

namespace Shos::MiniCadSample::Models {

using namespace Utilities;

class Figure : public CObject
{
    static constexpr long     selectorSize     = 10L;
    static constexpr long     selectorPenWidth = 5;
    static constexpr COLORREF selectedColor    = RGB(0x80, 0x00, 0x40);
    static constexpr COLORREF areaColor        = RGB(0x00, 0xa0, 0xff);
    
    FigureAttribute attribute;
    bool            isSelected = false;

public:
    const FigureAttribute& Attribute() const
    {
        return attribute;
    }

    FigureAttribute& Attribute()
    {
        return attribute;
    }

    Figure() = default;

    // Written by hand: CObject is not copyable, so these cannot be defaulted here (they can in the derived classes).
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

    // Draws the figure. The selection markers are drawn too if it is selected and drawSelector is true
    // (false for output such as the clipboard).
    void Draw(CDC& dc, bool drawSelector = true) const
    {
        StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
        CPen                pen(PS_SOLID, attribute.GetPenWidth(), attribute.GetColor());
        GdiObjectSelector   penSelector(dc, pen);

        DrawShape(dc);
        if (isSelected && drawSelector)
            DrawSelector(dc);
    }

    void DrawArea(CDC& dc) const
    {
        StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
        CPen                pen(PS_SOLID, 3, areaColor);
        GdiObjectSelector   penSelector(dc, pen);

        dc.Rectangle(GetArea());
    }

    CRect GetArea() const
    {
        auto area   = GetShapeArea();
        auto  margin = attribute.GetPenWidth() + selectorSize + selectorPenWidth;
        area.InflateRect(margin, margin);
        return area;
    }

    virtual long GetDistanceFrom(CPoint /* point */) const
    {
        return Geometry::maximumDistance;
    }
    
    virtual void Serialize(CArchive& ar) override
    {
        CObject::Serialize(ar);
        attribute.Serialize(ar);
    }

protected:
    virtual void DrawShape(CDC& /* dc */) const
    {}

    virtual CRect GetShapeArea() const
    {
        return CRect();
    }

    virtual std::vector<CPoint> GetPoints() const
    {
        return {};
    }
    
private:
    void DrawSelector(CDC& dc) const
    {
        StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
        CPen                pen(PS_SOLID, selectorPenWidth, selectedColor);
        GdiObjectSelector   penSelector(dc, pen);

        for (const auto& point : GetPoints())
            DrawSelector(dc, point);
    }

    void DrawSelector(CDC& dc, CPoint point) const
    {
        CRect rect(point, point);
        rect.InflateRect(selectorSize, selectorSize);
        dc.Rectangle(rect);
    }

    DECLARE_SERIAL(Figure)
};

class DotFigure : public Figure
{
    static constexpr long radius = 10L;
    CPoint                position;
        
public:
    DotFigure() = default;
    DotFigure(const DotFigure&) = default;
    DotFigure& operator =(const DotFigure&) = default;

    DotFigure(const CPoint& position) : position(position)
    {}

    virtual Figure* Clone() override
    {
        return new DotFigure(*this);
    }

    virtual long GetDistanceFrom(CPoint point) const override
    {
        return (std::max)(Geometry::GetDistance(point, position) - radius, 0L);
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

    virtual std::vector<CPoint> GetPoints() const override
    {
        return { position };
    }

    DECLARE_SERIAL(DotFigure)
};

class LineFigure : public Figure
{
    CPoint start, end;

public:
    LineFigure() = default;
    LineFigure(const LineFigure&) = default;
    LineFigure& operator =(const LineFigure&) = default;

    LineFigure(CPoint start, CPoint end) : start(start), end(end)
    {}

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

    virtual std::vector<CPoint> GetPoints() const override
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
    RectangleFigureBase() = default;
    RectangleFigureBase(const RectangleFigureBase&) = default;
    RectangleFigureBase& operator =(const RectangleFigureBase&) = default;

    RectangleFigureBase(const CRect& position) : position(position)
    {
        this->position.NormalizeRect();
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

    virtual std::vector<CPoint> GetPoints() const override
    {
        return Geometry::ToPoints(position);
    }

    DECLARE_SERIAL(RectangleFigureBase)
};

class RectangleFigure : public RectangleFigureBase
{
public:
    RectangleFigure() = default;
    RectangleFigure(const RectangleFigure&) = default;
    RectangleFigure& operator =(const RectangleFigure&) = default;

    RectangleFigure(const CRect& position) : RectangleFigureBase(position)
    {}

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
    EllipseFigure() = default;
    EllipseFigure(const EllipseFigure&) = default;
    EllipseFigure& operator =(const EllipseFigure&) = default;

    EllipseFigure(const CRect& position) : RectangleFigureBase(position)
    {}

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
    static constexpr long figureKindNumber = 4;

    inline static std::mt19937 mt{ std::random_device{}() };

public:
    static std::vector<Figure*> GetRandomFigures(size_t count, const CRect& area)
    {
        std::vector<Figure*> figures;
        figures.reserve(count);
        for (size_t counter = 0; counter < count; counter++) {
            auto figure = GetRandomFigure(area);
            if (figure != nullptr)
                figures.push_back(figure); // Does not throw: the capacity was reserved.
        }
        return figures;
    }

    static bool GetArea(const std::vector<Figure*>& figures, CRect& area)
    {
        if (figures.empty())
            return false;

        std::vector<CRect> areas(figures.size());
        std::ranges::transform(figures, areas.begin(), &Figure::GetArea);
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
            ASSERT(false); // A kind without a case above.
            return nullptr;
        }
        figure->Attribute().SetColor   (RandomColor()     );
        figure->Attribute().SetPenWidth(RandomValue(0, 5));
        return figure;
    }

    static CPoint RandomPosition(const CRect& area)
    {
        return CPoint(RandomValue(area.left, area.right), RandomValue(area.top, area.bottom));
    }

    // A uniformly distributed value in [minimum, maximum].
    static LONG RandomValue(long minimum, long maximum)
    {
        return std::uniform_int_distribution<long>(minimum, maximum)(mt);
    }

    static COLORREF RandomColor()
    {
        return RGB(RandomValue(0, 255), RandomValue(0, 255), RandomValue(0, 255));
    }
};

} // namespace Shos::MiniCadSample::Models
