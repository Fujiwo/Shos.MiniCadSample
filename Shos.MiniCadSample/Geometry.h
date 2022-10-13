#pragma once

#include <afx.h>
#include <vector>

#ifdef max
#undef max
#endif // max

class Geometry
{
public:
    template <class T>
    static T Square(T x)
    {
        return x * x;
    }
    static long Round(double x)
    {
        return static_cast<long>(floor(x + 0.5));
    }
	
    // absolute value
    static long Abs(CPoint point)
    {
        return Round(sqrt(static_cast<double>(Square(point.x) + Square(point.y))));
    }
	
    static long Abs(CSize size)
    {
        return Round(sqrt(static_cast<double>(Square(size.cx) + Square(size.cy))));
    }

    static long GetDistance(CPoint point1, CPoint point2)
    {
        return Abs(point2 - point1);
    }
	
    static long GetDistanceToLineSegment(CPoint point, CPoint start, CPoint end)
    {
        auto d  = end   - start;
        auto length = Abs(d);
        if (length == 0)
            return GetDistance(point, start);

        auto d1 = start - point;
        auto d2 = end   - point;

        auto f0 = Mul(d, d1);
        auto f1 = Mul(d, d2);

        return f0 > 0 ? Abs(d1)
            : (f1 < 0 ? Abs(d2) : abs(d.cy * d1.cx - d.cx * d1.cy) / length);
    }

    static long GetDistance(CPoint point, const CRect& rect)
    {
        auto points = ToPoints(rect);
        auto distance = std::numeric_limits<long>::max();

        for (size_t index = 0; index < points.size(); index++) {
            auto eachDistance = GetDistanceToLineSegment(point, points[index], points[(index + 1) % points.size()]);
			if (eachDistance < distance)
                distance = eachDistance;
        }
        return distance;
    }

    static long GetDistanceToEllipse(CPoint point, const CRect& rect)
    {
		if (rect.Width() == 0 || rect.Height() == 0)
			return GetDistanceToLineSegment(point, rect.TopLeft(), rect.BottomRight());

        auto rate = rect.Height() / (double)rect.Width();

		auto d = point - rect.CenterPoint();
		d.cx = Round(d.cx * rate);

		auto distance = Abs(d) - rect.Height() / 2;
        distance = Round(distance / rate);

        return distance;
    }

    // inner product
    static long Mul(CPoint point1, CPoint point2)
    {
        return point1.x * point2.x + point1.y * point2.y;
    }

    static long Mul(CSize size1, CSize size2)
    {
        return size1.cx * size2.cx + size1.cy * size2.cy;
    }

    static std::vector<CPoint> ToPoints(const CRect& rect)
    {
		return { rect.TopLeft(), CPoint(rect.right, rect.top), rect.BottomRight(), CPoint(rect.left, rect.bottom) };
    }
	
	static bool GetArea(const std::vector<CRect>& areas, CRect& area)
	{
		if (areas.size() == 0)
			return false;

		area = areas[0];
		for (auto index = 1; index < areas.size(); index++) {
			auto eachArea = areas[index];
			eachArea.NormalizeRect();
			area.UnionRect(area, eachArea);
		}
		return true;
	}
};
