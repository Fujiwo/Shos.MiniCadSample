#pragma once

// The main frame window (SDI). Owns the modeless figure attribute dialog and relays messages to it.

#include "FigureAttributeDialog.h"
#include "Observer.h"

namespace Shos::MiniCadSample::Views {

using namespace Models;
using namespace Utilities;

// The frame window. It owns the figure attribute dialog, and receives the two private messages below
// (sent through Application) so that other classes do not need to know the dialog.
class MainFrame : public CFrameWnd
{
    FigureAttributeDialog figureAttributeDialog;

public:
    // wParam: const FigureAttribute*. Shows the attribute in the dialog. Handled synchronously (SendMessage),
    // so the pointer need only live during the call.
    static constexpr UINT WM_SET_FIGURE_ATTRIBUTE          = WM_USER + 101;
    // wParam: Observer<FigureAttribute>*. Registers the observer of the dialog's changes (the pointer is kept).
    static constexpr UINT WM_SET_FIGURE_ATTRIBUTE_OBSERVER = WM_SET_FIGURE_ATTRIBUTE + 1;

protected:
    // Creates the (initially shown) modeless dialog.
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct)
    {
        if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
            return -1;

        figureAttributeDialog.Create(*this);
        return 0;
    }
    
    // The View menu item shows / hides the dialog, with a check mark that shows the state.
    afx_msg void OnViewFigureAttributeDialog()
    {
        ToggleWindow(figureAttributeDialog);
    }

    afx_msg void OnUpdateViewFigureAttributeDialog(CCmdUI* cmdUI)
    {
        cmdUI->SetCheck(figureAttributeDialog.IsWindowVisible() ? 1 : 0);
    }

    afx_msg LRESULT OnSetFigureAttribute(WPARAM wParam, LPARAM /* lParam */)
    {
        auto figureAttribute = reinterpret_cast<const FigureAttribute*>(wParam);
        figureAttributeDialog.Set(*figureAttribute);
        return 0L;
    }

    afx_msg LRESULT OnSetFigureAttributeObserver(WPARAM wParam, LPARAM /* lParam */)
    {
        auto figureAttributeObserver = reinterpret_cast<Observer<FigureAttribute>*>(wParam);
        figureAttributeDialog.AddObserver(*figureAttributeObserver);
        return 0L;
    }

private:
    // Hides window if it is visible, shows it otherwise.
    static void ToggleWindow(CWnd& window)
    {
        window.ShowWindow(window.IsWindowVisible() ? SW_HIDE : SW_SHOW);
    }

    DECLARE_DYNCREATE(MainFrame)
    DECLARE_MESSAGE_MAP()
};

} // namespace Shos::MiniCadSample::Views
