#include "pch.h"
#include "afxdialogex.h"
#include "FigureAttributeDialog.h"
#include "resource.h"

bool FigureAttributeDialog::Create(CWnd& parentWindow)
{
	if (CDialogEx::Create(IDD_FIGURE_ATTRIBUTE_DIALOG, &parentWindow)) {
		ShowWindow(SW_SHOW);
		return true;
	}
	return false;
}

void FigureAttributeDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_COLOR_BUTTON, colorButton);

	if (figureAttribute == nullptr)
		return;
	
	auto penWidth = figureAttribute->GetPenWidth();
	DDX_Text(pDX, IDC_PEN_WIDTH_EDIT, penWidth);
	figureAttribute->SetPenWidth(penWidth);

	if (pDX->m_bSaveAndValidate)
		figureAttribute->SetColor(colorButton.GetColor());
	else
		colorButton.SetColor(figureAttribute->GetColor());
}

int FigureAttributeDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) {
	case IDC_COLOR_BUTTON	 :
	case IDC_PEN_WIDTH_BUTTON:
		UpdateData(true);
		break;
	}
	return 0;
}

IMPLEMENT_DYNAMIC(FigureAttributeDialog, CDialogEx)

BEGIN_MESSAGE_MAP(FigureAttributeDialog, CDialogEx)
END_MESSAGE_MAP()
