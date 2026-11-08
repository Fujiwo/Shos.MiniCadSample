#pragma once

// The application object (MFC CWinApp), and static helpers that reach the main frame.

#ifndef __AFXWIN_H__
    #error "Include 'pch.h' before including this file for PCH"
#endif

#include "MainFrame.h"

namespace Shos::MiniCadSample {

// The MFC application. There is one global instance, theApp (Application.cpp).
// The static functions let the view and the model side talk to the figure attribute dialog
// without knowing it: they send messages to the main frame (see MainFrame), and do nothing if it does not exist.
class Application : public CWinApp
{
public:
    // Shows figureAttribute in the dialog. The dialog copies it.
    static void Set(const Models::FigureAttribute& figureAttribute)
    {
        if (auto mainFrame = GetMainFrame())
            mainFrame->SendMessage(Views::MainFrame::WM_SET_FIGURE_ATTRIBUTE, reinterpret_cast<WPARAM>(&figureAttribute));
    }

    // Registers observer for the changes the user makes in the dialog. observer must outlive the dialog.
    static void SetFigureAttributeObserver(Utilities::Observer<Models::FigureAttribute>& observer)
    {
        if (auto mainFrame = GetMainFrame())
            mainFrame->SendMessage(Views::MainFrame::WM_SET_FIGURE_ATTRIBUTE_OBSERVER, reinterpret_cast<WPARAM>(&observer));
    }
    
    // Creates the SDI document template and opens the main window.
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
