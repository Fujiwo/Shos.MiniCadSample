// Runtime class information of the commands (the command classes are otherwise header-only).
#include "pch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Command.h"

namespace Shos::MiniCadSample::Controllers {

IMPLEMENT_DYNCREATE(Command, CObject)
IMPLEMENT_DYNCREATE(SelectCommand, Command)
IMPLEMENT_DYNCREATE(DotCommand, Command)
IMPLEMENT_DYNCREATE(LineCommand, Command)
IMPLEMENT_DYNCREATE(RectangleCommand, Command)
IMPLEMENT_DYNCREATE(EllipseCommand, Command)

} // namespace Shos::MiniCadSample::Controllers
