#pragma once

#ifndef __AFXWIN_H__
	#error "Include 'pch.h' before including this file for PCH"
#endif

#include "MainFrame.h"

class Application : public CWinApp
{
public:
	static void Set(const FigureAttribute& figureAttribute)
	{
		auto mainFrame = static_cast<MainFrame*>(::AfxGetMainWnd());
		if (::IsWindow(mainFrame->GetSafeHwnd()))
			mainFrame->SendMessage(MainFrame::WM_SET_FIGURE_ATTRIBUTE, reinterpret_cast<WPARAM>(&figureAttribute));
	}

	static void SetFigureAttributeObserver(Observer<FigureAttribute>& observer)
	{
		auto mainFrame = static_cast<MainFrame*>(::AfxGetMainWnd());
		if (::IsWindow(mainFrame->GetSafeHwnd()))
			mainFrame->SendMessage(MainFrame::WM_SET_FIGURE_ATTRIBUTE_OBSERVER, reinterpret_cast<WPARAM>(&observer));
	}
	
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
