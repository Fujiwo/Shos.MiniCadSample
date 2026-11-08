// Tests of FigureAttribute and Geometry.

#include "pch.h"
#include "CppUnitTest.h"
#include "../Shos.MiniCadSample/FigureAttribute.h"
#include "../Shos.MiniCadSample/Geometry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Shos::MiniCadSample::Models;
using namespace Shos::MiniCadSample::Utilities;

namespace ShosMiniCadSampleTest
{
    FigureAttribute make_attribute(COLORREF color, int penWidth)
    {
        FigureAttribute attribute;
        attribute.SetColor(color);
        attribute.SetPenWidth(penWidth);
        return attribute;
    }

    TEST_CLASS(figure_attribute_test)
    {
    public:
        TEST_METHOD(default_attribute_is_valid)
        {
            const FigureAttribute attribute;
            Assert::IsTrue(attribute.IsColorValid());
            Assert::IsTrue(attribute.IsPenWidthValid());
            Assert::IsFalse(attribute.IsEmpty());
        }

        TEST_METHOD(empty_has_no_valid_item)
        {
            const auto empty = FigureAttribute::Empty();
            Assert::IsFalse(empty.IsColorValid());
            Assert::IsFalse(empty.IsPenWidthValid());
            Assert::IsTrue(empty.IsEmpty());
        }

        TEST_METHOD(set_makes_item_valid)
        {
            auto attribute = FigureAttribute::Empty();
            Assert::IsTrue(attribute.SetColor(RGB(0, 0, 0))); // Changed: invalid -> valid, even with the same value.
            Assert::IsTrue(attribute.IsColorValid());
            Assert::IsFalse(attribute.IsPenWidthValid());
            Assert::IsFalse(attribute.SetColor(RGB(0, 0, 0)));

            Assert::IsTrue(attribute.SetPenWidth(3));
            Assert::IsTrue(attribute.IsPenWidthValid());
            Assert::AreEqual(3, attribute.GetPenWidth());
            Assert::IsFalse(attribute.SetPenWidth(3));
        }

        TEST_METHOD(get_sum_of_nothing)
        {
            const auto sum = FigureAttribute::GetSum({});
            Assert::IsTrue(sum.IsEmpty());
        }

        TEST_METHOD(get_sum_of_same_values)
        {
            const auto sum = FigureAttribute::GetSum({ make_attribute(RGB(1, 2, 3), 2), make_attribute(RGB(1, 2, 3), 2) });
            Assert::IsTrue(sum.IsColorValid());
            Assert::IsTrue(sum.IsPenWidthValid());
            Assert::AreEqual<COLORREF>(RGB(1, 2, 3), sum.GetColor());
            Assert::AreEqual(2, sum.GetPenWidth());
        }

        TEST_METHOD(get_sum_of_mixed_values)
        {
            const auto sum = FigureAttribute::GetSum({
                make_attribute(RGB(1, 2, 3), 1), make_attribute(RGB(1, 2, 3), 3), make_attribute(RGB(1, 2, 3), 1)
            });
            Assert::IsTrue(sum.IsColorValid());
            Assert::AreEqual<COLORREF>(RGB(1, 2, 3), sum.GetColor());
            Assert::IsFalse(sum.IsPenWidthValid());

            const auto colorSum = FigureAttribute::GetSum({ make_attribute(RGB(1, 2, 3), 1), make_attribute(RGB(9, 9, 9), 1) });
            Assert::IsFalse(colorSum.IsColorValid());
            Assert::IsTrue(colorSum.IsPenWidthValid());
        }

        TEST_METHOD(apply_to_overwrites_valid_items_only)
        {
            auto target = make_attribute(RGB(1, 2, 3), 1);

            auto colorChange = FigureAttribute::Empty();
            colorChange.SetColor(RGB(9, 9, 9));
            Assert::IsTrue(colorChange.ApplyTo(target));
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), target.GetColor());
            Assert::AreEqual(1, target.GetPenWidth());
            Assert::IsFalse(colorChange.ApplyTo(target)); // Already applied.

            auto penWidthChange = FigureAttribute::Empty();
            penWidthChange.SetPenWidth(4);
            Assert::IsTrue(penWidthChange.ApplyTo(target));
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), target.GetColor());
            Assert::AreEqual(4, target.GetPenWidth());

            Assert::IsFalse(FigureAttribute::Empty().ApplyTo(target));
            Assert::IsTrue(target.IsColorValid());
            Assert::IsTrue(target.IsPenWidthValid());
        }

        TEST_METHOD(apply_to_makes_target_item_valid)
        {
            // The dialog shows the sum of mixed values (invalid); a change makes the item valid.
            auto shown = FigureAttribute::GetSum({ make_attribute(RGB(1, 2, 3), 1), make_attribute(RGB(9, 9, 9), 3) });
            auto change = FigureAttribute::Empty();
            change.SetColor(RGB(1, 2, 3)); // The same value as the first one.
            Assert::IsTrue(change.ApplyTo(shown));
            Assert::IsTrue(shown.IsColorValid());
            Assert::IsFalse(shown.IsPenWidthValid());
        }

        // H3: a color change of figures with different pen widths must keep each pen width.
        TEST_METHOD(color_change_keeps_mixed_pen_widths)
        {
            auto first  = make_attribute(RGB(1, 2, 3), 1);
            auto second = make_attribute(RGB(4, 5, 6), 3);
            auto shown  = FigureAttribute::GetSum({ first, second });
            Assert::IsFalse(shown.IsPenWidthValid());

            auto change = FigureAttribute::Empty();
            change.SetColor(RGB(9, 9, 9));
            change.ApplyTo(first);
            change.ApplyTo(second);
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), first.GetColor());
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), second.GetColor());
            Assert::AreEqual(1, first .GetPenWidth());
            Assert::AreEqual(3, second.GetPenWidth());
        }
    };

    TEST_CLASS(geometry_test)
    {
    public:
        TEST_METHOD(distance_between_points)
        {
            Assert::AreEqual(5L, Geometry::GetDistance(CPoint(1, 2), CPoint(4, 6)));
            Assert::AreEqual(0L, Geometry::GetDistance(CPoint(1, 2), CPoint(1, 2)));
        }

        TEST_METHOD(distance_to_line_segment)
        {
            const CPoint start(0, 0), end(100, 0);
            Assert::AreEqual(30L, Geometry::GetDistanceToLineSegment(CPoint( 50, 30), start, end)); // Perpendicular.
            Assert::AreEqual( 0L, Geometry::GetDistanceToLineSegment(CPoint( 70,  0), start, end)); // On the segment.
            Assert::AreEqual(50L, Geometry::GetDistanceToLineSegment(CPoint(-30, 40), start, end)); // Beyond the start.
            Assert::AreEqual(50L, Geometry::GetDistanceToLineSegment(CPoint(130, 40), start, end)); // Beyond the end.
            Assert::AreEqual( 5L, Geometry::GetDistanceToLineSegment(CPoint( 13, 14), CPoint(10, 10), CPoint(10, 10))); // A point.
        }

        TEST_METHOD(distance_to_rectangle)
        {
            const CRect rect(0, 0, 100, 50);
            Assert::AreEqual(10L, Geometry::GetDistance(CPoint( 50, 60), rect)); // Outside, below.
            Assert::AreEqual(20L, Geometry::GetDistance(CPoint( 50, 20), rect)); // Inside: to the nearest side.
            Assert::AreEqual( 5L, Geometry::GetDistance(CPoint(103, 54), rect)); // Outside, near a corner.
            Assert::AreEqual( 0L, Geometry::GetDistance(CPoint(100, 25), rect)); // On a side.
        }

        TEST_METHOD(distance_to_circle)
        {
            const CRect rect(0, 0, 100, 100);
            Assert::AreEqual( 0L, Geometry::GetDistanceToEllipse(CPoint(100,  50), rect)); // On the circle.
            Assert::AreEqual(50L, Geometry::GetDistanceToEllipse(CPoint( 50,  50), rect)); // The center.
            Assert::AreEqual(50L, Geometry::GetDistanceToEllipse(CPoint(150,  50), rect)); // Outside.
            Assert::AreEqual(30L, Geometry::GetDistanceToEllipse(CPoint( 50, 130), rect));
        }

        TEST_METHOD(distance_to_ellipse)
        {
            // An approximation: exact on the circle itself and along the major axis.
            const CRect rect(0, 0, 200, 100);
            Assert::AreEqual(  0L, Geometry::GetDistanceToEllipse(CPoint(200,  50), rect)); // End of the major axis.
            Assert::AreEqual(  0L, Geometry::GetDistanceToEllipse(CPoint(100,   0), rect)); // End of the minor axis.
            Assert::AreEqual(100L, Geometry::GetDistanceToEllipse(CPoint(300,  50), rect)); // Along the major axis.
        }

        TEST_METHOD(distance_to_flat_ellipse)
        {
            // A flat ellipse is a line segment.
            Assert::AreEqual(10L, Geometry::GetDistanceToEllipse(CPoint(50, 10), CRect(0, 0, 100, 0)));
        }

        TEST_METHOD(clamp_point_into_area)
        {
            const CRect area(10, 20, 110, 220);
            Assert::IsTrue(CPoint( 50,  60) == Geometry::Clamp(CPoint( 50,  60), area)); // Inside: unchanged.
            Assert::IsTrue(CPoint( 10,  20) == Geometry::Clamp(CPoint(-50, -60), area)); // Above left.
            Assert::IsTrue(CPoint(109, 219) == Geometry::Clamp(CPoint(500, 600), area)); // Below right.
            Assert::IsTrue(CPoint(109,  60) == Geometry::Clamp(CPoint(110,  60), area)); // On the right edge (outside).
            Assert::IsTrue(CPoint( 50,  20) == Geometry::Clamp(CPoint( 50,   0), area));

            // The result is always in the area, as PtInRect tells.
            for (auto point : { CPoint(-1, -1), CPoint(110, 220), CPoint(1000, 30), CPoint(30, 1000) })
                Assert::IsTrue(area.PtInRect(Geometry::Clamp(point, area)) != FALSE);
        }

        TEST_METHOD(area_of_rectangles)
        {
            CRect area;
            Assert::IsFalse(Geometry::GetArea({}, area));

            Assert::IsTrue(Geometry::GetArea({ CRect(10, 10, 20, 20), CRect(50, 40, 30, 5) /* not normalized */ }, area));
            Assert::IsTrue(area == CRect(10, 5, 50, 40));

            // The first one is normalized too.
            Assert::IsTrue(Geometry::GetArea({ CRect(20, 20, 10, 10) }, area));
            Assert::IsTrue(area == CRect(10, 10, 20, 20));
        }

        TEST_METHOD(rectangle_in_rectangle)
        {
            const CRect outer(10, 10, 100, 100);
            Assert::IsTrue (Geometry::InRect(outer, CRect( 20,  20,  30,  30)));
            Assert::IsTrue (Geometry::InRect(outer, CRect( 10,  10, 100, 100))); // Touching all the edges.
            Assert::IsTrue (Geometry::InRect(outer, CRect( 50,  50, 100, 100))); // Touching the right and bottom.
            Assert::IsTrue (Geometry::InRect(outer, CRect( 30,  30,  20,  20))); // Not normalized.
            Assert::IsFalse(Geometry::InRect(outer, CRect(  5,  20,  30,  30)));
            Assert::IsFalse(Geometry::InRect(outer, CRect( 20,  20, 101,  30)));
            Assert::IsFalse(Geometry::InRect(outer, CRect( 20,  20,  30, 101)));
            Assert::IsTrue (Geometry::InRect(CRect(100, 100, 10, 10), CRect(20, 20, 30, 30))); // Outer not normalized.
        }
    };
}
