#pragma once

#include "afxdialogex.h"
#include "FigureAttribute.h"

// A modeless dialog that shows the attribute of the selected figures (or of new figures)
// and notifies its observers (Model) of each change the user makes.
// A notification is a change: only the changed item is valid (see FigureAttribute::ApplyTo),
// so that items the user did not touch (e.g. different pen widths of the selected figures) are kept.
class FigureAttributeDialog : public CDialogEx, public Observable<FigureAttribute>
{
    FigureAttribute figureAttribute; // What the dialog shows. An invalid item is shown blank.
    CMFCColorButton colorButton;
    
public:
    FigureAttributeDialog()
    {}

    void Set(const FigureAttribute& newFigureAttribute)
    {
        figureAttribute = newFigureAttribute;
        UpdateData(false);
    }

    bool Create(CWnd& parentWindow);

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual int OnCommand(WPARAM wParam, LPARAM lParam) override;
    
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

    void SaveColor()
    {
        auto change = FigureAttribute::Empty();
        change.SetColor(colorButton.GetColor());
        Notify(change);
    }

    void SavePenWidth();

    void Load(CDataExchange* pDX)
    {
        LoadColor();
        LoadPenWidth(pDX);
    }

    void LoadColor()
    {
        if (figureAttribute.IsColorValid())
            colorButton.SetColor(figureAttribute.GetColor());
        else
            colorButton.SetColor(colorButton.GetAutomaticColor());
    }

    void LoadPenWidth(CDataExchange* pDX);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_FIGURE_ATTRIBUTE_DIALOG };
#endif
};
