#include "pch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "ClipboardHelper.h"

HGLOBAL ClipboardHelper::globalMemoryHandle = nullptr;
