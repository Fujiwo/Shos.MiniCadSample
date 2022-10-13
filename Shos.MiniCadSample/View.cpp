#include "pch.h"
#include "framework.h"
#include "Document.h"
#include "View.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROYCLIPBOARD()
END_MESSAGE_MAP()
