#include "pch.h"
#include "Figure.h"

IMPLEMENT_SERIAL(Figure, CObject, 1)
IMPLEMENT_SERIAL(DotFigure, Figure, 1)
IMPLEMENT_SERIAL(LineFigure, Figure, 1)
IMPLEMENT_SERIAL(RectangleFigureBase, Figure, 1)
IMPLEMENT_SERIAL(RectangleFigure, RectangleFigureBase, 1)
IMPLEMENT_SERIAL(EllipseFigure, RectangleFigureBase, 1)

std::random_device FigureHelper::random;
std::mt19937 FigureHelper::mt(random());
