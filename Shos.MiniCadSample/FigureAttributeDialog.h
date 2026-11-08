#pragma once

// The modeless dialog to show and edit the figure attribute (color and pen width).

#include "afxdialogex.h"
#include "FigureAttribute.h"

namespace Shos::MiniCadSample::Views {

using namespace Models;
using namespace Utilities;

// A modeless dialog that shows the attribute of the selected figures (or of new figures)
// and notifies its observers (Model) of each change the user makes.
// A notification is a change: only the changed item is valid (see FigureAttribute::ApplyTo),
// so that items the user did not touch (e.g. different pen widths of the selected figures) are kept.
class FigureAttributeDialog : public CDialogEx, public Observable<FigureAttribute>
{
    FigureAttribute figureAttribute; // What the dialog shows. An invalid item is shown blank.
    CMFCColorButton colorButton; // The color picker (control IDC_COLOR_BUTTON).
    
public:
    FigureAttributeDialog()
    {}

    // Shows newFigureAttribute (does not notify the observers: this is not a user's change).
    void Set(const FigureAttribute& newFigureAttribute)
    {
        figureAttribute = newFigureAttribute;
        UpdateData(false);
    }

    // Creates the dialog as a modeless child of parentWindow and shows it. Returns false on failure.
    bool Create(CWnd& parentWindow);

protected:
    // Only loads (model to controls). Reading controls is done item by item in OnCommand.
    virtual void DoDataExchange(CDataExchange* pDX) override;
    // Reacts to the color button and the pen width button.
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;

    // Enter: applies the pen width. The modeless dialog must not be ended (EndDialog) by CDialog::OnOK.
    virtual void OnOK() override
    {
        SavePenWidth();
    }

    // The close button and Esc (both come as IDCANCEL): hides the dialog, so that it can be shown again.
    virtual void OnCancel() override
    {
        ShowWindow(SW_HIDE);
    }

    DECLARE_DYNAMIC(FigureAttributeDialog)
    DECLARE_MESSAGE_MAP()

private:
    // Notifies the observers of change, if it changes what the dialog shows.
    void Notify(const FigureAttribute& change)
    {
        if (change.ApplyTo(figureAttribute))
            NotifyObservers(change);
    }

    // Reads the color from the control, and notifies it as a change.
    void SaveColor()
    {
        auto change = FigureAttribute::Empty();
        change.SetColor(colorButton.GetColor());
        Notify(change);
    }

    // Reads the pen width from the edit box, and notifies it as a change (does nothing if blank or invalid).
    void SavePenWidth();

    // Copies figureAttribute to the controls.
    void Load(CDataExchange* pDX)
    {
        LoadColor();
        LoadPenWidth(pDX);
    }

    // An invalid (mixed) color is shown as the automatic color.
    void LoadColor()
    {
        if (figureAttribute.IsColorValid())
            colorButton.SetColor(figureAttribute.GetColor());
        else
            colorButton.SetColor(colorButton.GetAutomaticColor());
    }

    // An invalid (mixed) pen width is shown blank.
    void LoadPenWidth(CDataExchange* pDX);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_FIGURE_ATTRIBUTE_DIALOG };
#endif
};

} // namespace Shos::MiniCadSample::Views
