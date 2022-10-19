#include "pch.h"
#include "framework.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "MainFrame.h"
#include "resource.h"

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_COMMAND(ID_VIEW_FIGURE_ATTRIBUTE_DIALOG, OnViewFigureAttributeDialog)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIGURE_ATTRIBUTE_DIALOG, OnUpdateViewFigureAttributeDialog)
	ON_MESSAGE(WM_SET_FIGURE_ATTRIBUTE, OnSetFigureAttribute)
	ON_MESSAGE(WM_SET_FIGURE_ATTRIBUTE_OBSERVER, OnSetFigureAttributeObserver)
END_MESSAGE_MAP()
