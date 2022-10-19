#include "pch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Figure.h"

#ifdef max
#undef max
#endif // max

IMPLEMENT_SERIAL(Figure, CObject, 1)
IMPLEMENT_SERIAL(DotFigure, Figure, 1)
IMPLEMENT_SERIAL(LineFigure, Figure, 1)
IMPLEMENT_SERIAL(RectangleFigureBase, Figure, 1)
IMPLEMENT_SERIAL(RectangleFigure, RectangleFigureBase, 1)
IMPLEMENT_SERIAL(EllipseFigure, RectangleFigureBase, 1)

long Figure::GetDistanceFrom(CPoint point) const
{
    return std::numeric_limits<long>::max();
}

std::random_device FigureHelper::random;
std::mt19937 FigureHelper::mt(random());
