#pragma once

#ifndef __AFXWIN_H__
	#error "Include 'pch.h' before including this file for PCH"
#endif

#include "MainFrame.h"

class Application : public CWinApp
{
public:
	static void Set(FigureAttribute& figureAttribute)
	{
		auto mainFrame = static_cast<MainFrame*>(::AfxGetMainWnd());
		mainFrame->SendMessage(MainFrame::WM_SET_FIGURE_ATTRIBUTE, reinterpret_cast<WPARAM>(&figureAttribute));
	}
	
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
