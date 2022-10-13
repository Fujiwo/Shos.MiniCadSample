#pragma once

#include "FigureAttributeDialog.h"

class MainFrame : public CFrameWnd
{
	FigureAttributeDialog figureAttributeDialog;

public:
	static const UINT WM_SET_FIGURE_ATTRIBUTE	  = WM_USER + 101;
	//static const UINT WM_FIGURE_ATTRIBUTE_CHANGED = WM_SET_FIGURE_ATTRIBUTE + 1;

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
			return -1;

		figureAttributeDialog.Create(*this);
		return 0;
	}
	
	afx_msg void OnViewFigureAttributeDialog()
	{
		ToggleWindow(figureAttributeDialog);
	}

	afx_msg void OnUpdateViewFigureAttributeDialog(CCmdUI* cmdUI)
	{
		cmdUI->SetCheck(figureAttributeDialog.IsWindowVisible() ? 1 : 0);
	}

	LRESULT OnSetFigureAttribute(WPARAM wParam, LPARAM lParam)
	{
		FigureAttribute* figureAttribute = reinterpret_cast<FigureAttribute*>(wParam);
		figureAttributeDialog.Set(*figureAttribute);
		return 0L;
	}

	//LRESULT OnFigureAttributeChanged(WPARAM wParam, LPARAM lParam)
	//{
	//	FigureAttribute* figureAttribute = reinterpret_cast<FigureAttribute*>(wParam);

	//	return 0L;
	//}

private:
	static void ToggleWindow(CWnd& window)
	{
		window.ShowWindow(window.IsWindowVisible() ? SW_HIDE : SW_SHOW);
	}

	DECLARE_DYNCREATE(MainFrame)
	DECLARE_MESSAGE_MAP()
};
