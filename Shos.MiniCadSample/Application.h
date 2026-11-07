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
        if (auto mainFrame = GetMainFrame())
            mainFrame->SendMessage(MainFrame::WM_SET_FIGURE_ATTRIBUTE, reinterpret_cast<WPARAM>(&figureAttribute));
    }

    static void SetFigureAttributeObserver(Observer<FigureAttribute>& observer)
    {
        if (auto mainFrame = GetMainFrame())
            mainFrame->SendMessage(MainFrame::WM_SET_FIGURE_ATTRIBUTE_OBSERVER, reinterpret_cast<WPARAM>(&observer));
    }
    
    virtual BOOL InitInstance();

private:
    // Returns the main frame, or nullptr if it does not exist (yet / any more), e.g. during startup or shutdown.
    static MainFrame* GetMainFrame()
    {
        auto mainWindow = ::AfxGetMainWnd();
        if (mainWindow == nullptr || !::IsWindow(mainWindow->GetSafeHwnd()))
            return nullptr;
        return static_cast<MainFrame*>(mainWindow);
    }

public:

    DECLARE_MESSAGE_MAP()
};
