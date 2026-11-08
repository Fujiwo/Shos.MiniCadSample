#include "pch.h"
#include <limits>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Geometry.h"

#ifdef max
#undef max
#endif // max

namespace Shos::MiniCadSample::Utilities {

const long Geometry::maximumDistance = std::numeric_limits<long>::max();

} // namespace Shos::MiniCadSample::Utilities
