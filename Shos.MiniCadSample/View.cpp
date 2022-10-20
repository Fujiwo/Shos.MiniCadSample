#include "pch.h"
#include "framework.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Document.h"
#include "View.h"

#ifdef SCROLL_VIEW
IMPLEMENT_DYNCREATE(View, DoubleBufferScrollView)
#else // SCROLL_VIEW
IMPLEMENT_DYNCREATE(View, DoubleBufferView)
#endif // SCROLL_VIEW 

#ifdef SCROLL_VIEW
BEGIN_MESSAGE_MAP(View, DoubleBufferScrollView)
#else // SCROLL_VIEW
BEGIN_MESSAGE_MAP(View, DoubleBufferView)
#endif // SCROLL_VIEW 
    ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
    ON_COMMAND(ID_EDIT_CUT, OnEditCut)
    ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
#ifdef ZOOMING_VIEW
    ON_WM_MOUSEWHEEL()
#endif // ZOOMING_VIEW 
    ON_WM_DESTROYCLIPBOARD()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
