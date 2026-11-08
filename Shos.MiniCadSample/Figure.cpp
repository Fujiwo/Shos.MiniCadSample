// Serialization registration (IMPLEMENT_SERIAL) of the figure classes. The figures are implemented in Figure.h.
// The class names are stored unqualified in files and on the clipboard, so the namespace does not change the format.

#include "pch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Figure.h"

// <windows.h> defines a max macro that breaks std::max in the headers.
#ifdef max
#undef max
#endif // max

namespace Shos::MiniCadSample::Models {

// The last argument is the schema version of the file format.
IMPLEMENT_SERIAL(Figure, CObject, 1)
IMPLEMENT_SERIAL(DotFigure, Figure, 1)
IMPLEMENT_SERIAL(LineFigure, Figure, 1)
IMPLEMENT_SERIAL(RectangleFigureBase, Figure, 1)
IMPLEMENT_SERIAL(RectangleFigure, RectangleFigureBase, 1)
IMPLEMENT_SERIAL(EllipseFigure, RectangleFigureBase, 1)

} // namespace Shos::MiniCadSample::Models
