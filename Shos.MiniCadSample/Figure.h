#pragma once

// Figure classes (dot, line, rectangle, ellipse): drawing, hit testing, serialization and cloning.
// FigureHelper creates random figures. All coordinates are logical coordinates.

#include <afx.h>
#include <algorithm>
#include <random>
#include <vector>
#include "FigureAttribute.h"
#include "GdiObjectSelector.h"
#include "Geometry.h"

namespace Shos::MiniCadSample::Models {

using namespace Utilities;

// Base class of all figures. Serializable (DECLARE_SERIAL) and cloneable.
// A figure that has been given to Model is owned by it and must not be changed in place (except by Select):
// to change it, Clone() it, edit the clone and replace the original (see Model::Update), so that it is undoable.
class Figure : public CObject
{
    static constexpr long     selectorSize     = 10L;      // Half the side of a selection marker.
    static constexpr long     selectorPenWidth = 5;
    static constexpr COLORREF selectedColor    = RGB(0x80, 0x00, 0x40);
    static constexpr COLORREF areaColor        = RGB(0x00, 0xa0, 0xff);
    
    FigureAttribute attribute;
    bool            isSelected = false; // Not serialized: a loaded figure starts unselected.

public:
    // The color and pen width of the figure. Always fully valid (see FigureAttribute).
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

    // Sets the selection state in place. This is the one change that is not recorded for undo.
    void Select(bool selected)
    {
        this->isSelected = selected;
    }

    // Returns a new copy of the same dynamic type, including the attribute and the selection state.
    // The caller owns the copy (normally it is handed to Model).
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

    // Draws the outline of GetArea() (a debugging aid).
    void DrawArea(CDC& dc) const
    {
        StockObjectSelector stockObjectSelector(dc, NULL_BRUSH);
        CPen                pen(PS_SOLID, 3, areaColor);
        GdiObjectSelector   penSelector(dc, pen);

        dc.Rectangle(GetArea());
    }

    // The bounding rectangle that the figure can paint on: the shape area enlarged by the pen width and
    // the selection markers. Used for redrawing and for selecting by a rectangle.
    CRect GetArea() const
    {
        auto area   = GetShapeArea();
        auto  margin = attribute.GetPenWidth() + selectorSize + selectorPenWidth;
        area.InflateRect(margin, margin);
        return area;
    }

    // The distance from point to the outline of the figure, used for picking (0 means on it).
    // The base class returns the maximum distance, so that a figure without a shape is never picked.
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
    // Draws only the shape. The pen and the null brush are already selected into dc by Draw.
    virtual void DrawShape(CDC& /* dc */) const
    {}

    // The bounding rectangle of the shape itself (normalized, without the pen width or markers).
    virtual CRect GetShapeArea() const
    {
        return CRect();
    }

    // The points where the selection markers are drawn (end points or corners).
    virtual std::vector<CPoint> GetPoints() const
    {
        return {};
    }
    
private:
    // Draws a marker on each of GetPoints().
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

// A filled-looking small circle (drawn as an outline) of a fixed radius around position.
class DotFigure : public Figure
{
    static constexpr long radius = 10L;
    CPoint                position; // The center.
        
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

    // The distance to the circle, 0 inside it.
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

// A line segment from start to end.
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

// Common base of RectangleFigure and EllipseFigure: both are defined by a normalized bounding rectangle.
// The derived classes provide DrawShape, Clone and GetDistanceFrom.
class RectangleFigureBase : public Figure
{
protected:
    CRect position; // Normalized (left <= right, top <= bottom).

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

// A rectangle outline.
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

    virtual long GetDistanceFrom(CPoint point) const override
    {
        return Geometry::GetDistance(point, position);
    }

protected:
    virtual void DrawShape(CDC& dc) const override
    {
        dc.Rectangle(&position);
    }

    DECLARE_SERIAL(RectangleFigure)
};

// An ellipse outline inscribed in the bounding rectangle.
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

    virtual long GetDistanceFrom(CPoint point) const override
    {
        return Geometry::GetDistanceToEllipse(point, position);
    }

protected:
    virtual void DrawShape(CDC& dc) const override
    {
        dc.Ellipse(&position);
    }

    DECLARE_SERIAL(EllipseFigure)
};

// Helpers that work on figures: random figure generation (for sample data) and the area of a set of figures.
class FigureHelper
{
    static constexpr long figureKindNumber = 4; // Dot, line, rectangle, ellipse: the cases of GetRandomFigure.

    inline static std::mt19937 mt{ std::random_device{}() };

public:
    // Creates count random figures inside area, with random colors and pen widths.
    // The caller owns the returned figures.
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

    // Sets area to the union of the GetArea() of the figures. Returns false (area untouched) if there are none.
    static bool GetArea(const std::vector<Figure*>& figures, CRect& area)
    {
        if (figures.empty())
            return false;

        std::vector<CRect> areas(figures.size());
        std::ranges::transform(figures, areas.begin(), &Figure::GetArea);
        return Geometry::GetArea(areas, area);
    }

private:
    // Creates one random figure, or returns nullptr (never in practice) if the kind has no case.
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
