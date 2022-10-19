#pragma once

#include "afxdialogex.h"
#include "FigureAttribute.h"

class FigureAttributeDialog : public CDialogEx, public Observable<FigureAttribute>
{
    FigureAttribute figureAttribute;
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
    void Save(CDataExchange* pDX)
    {
        SaveColor();
        SavePenWidth(pDX);
    }

    void SaveColor()
    {
        if (figureAttribute.SetColor(colorButton.GetColor()))
            NotifyObservers(figureAttribute);
    }

    void SavePenWidth(CDataExchange* pDX);

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
