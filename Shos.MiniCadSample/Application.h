#pragma once

#ifndef __AFXWIN_H__
    #error "Include 'pch.h' before including this file for PCH"
#endif

#include "MainFrame.h"

namespace Shos::MiniCadSample {

class Application : public CWinApp
{
public:
    static void Set(const Models::FigureAttribute& figureAttribute)
    {
        if (auto mainFrame = GetMainFrame())
            mainFrame->SendMessage(Views::MainFrame::WM_SET_FIGURE_ATTRIBUTE, reinterpret_cast<WPARAM>(&figureAttribute));
    }

    static void SetFigureAttributeObserver(Utilities::Observer<Models::FigureAttribute>& observer)
    {
        if (auto mainFrame = GetMainFrame())
            mainFrame->SendMessage(Views::MainFrame::WM_SET_FIGURE_ATTRIBUTE_OBSERVER, reinterpret_cast<WPARAM>(&observer));
    }
    
    virtual BOOL InitInstance() override;

private:
    // Returns the main frame, or nullptr if it does not exist (yet / any more), e.g. during startup or shutdown.
    static Views::MainFrame* GetMainFrame()
    {
        auto mainWindow = ::AfxGetMainWnd();
        if (mainWindow == nullptr || !::IsWindow(mainWindow->GetSafeHwnd()))
            return nullptr;
        return static_cast<Views::MainFrame*>(mainWindow);
    }

public:

    DECLARE_MESSAGE_MAP()
};

} // namespace Shos::MiniCadSample
