#include "pch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Command.h"

IMPLEMENT_DYNCREATE(Command, CObject)
IMPLEMENT_DYNCREATE(SelectCommand, Command)
IMPLEMENT_DYNCREATE(DotCommand, Command)
IMPLEMENT_DYNCREATE(LineCommand, Command)
IMPLEMENT_DYNCREATE(RectangleCommand, Command)
IMPLEMENT_DYNCREATE(EllipseCommand, Command)
