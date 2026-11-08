// DoubleBuffer.cpp : Definitions of the runtime class information and message maps.
// All class implementations are in DoubleBuffer.h.

#include "pch.h"
#include "framework.h"
#include "DoubleBuffer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace Shos::MFCDoubleBuffer {

// DoubleBufferViewBase is a template and cannot have a message map, so the common handlers
// (OnPaint / OnEraseBkgnd / OnSize) are registered in the map of each derived class.

IMPLEMENT_DYNCREATE(DoubleBufferView, DoubleBufferViewBase<CView>)

BEGIN_MESSAGE_MAP(DoubleBufferView, DoubleBufferViewBase<CView>)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
END_MESSAGE_MAP()

IMPLEMENT_DYNCREATE(DoubleBufferScrollView, DoubleBufferViewBase<CScrollView>)

// Scrolling is handled by OnScrollBy (a virtual function), so WM_HSCROLL and similar need not be registered.
BEGIN_MESSAGE_MAP(DoubleBufferScrollView, DoubleBufferViewBase<CScrollView>)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
END_MESSAGE_MAP()

} // namespace Shos::MFCDoubleBuffer
