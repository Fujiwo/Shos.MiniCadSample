﻿#include "pch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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

	if (pDX->m_bSaveAndValidate) {
		if (figureAttribute.SetColor(colorButton.GetColor()))
			NotifyObservers(figureAttribute);

		CString penWidthText;
		GetDlgItemText(IDC_PEN_WIDTH_EDIT, penWidthText);
		if (!penWidthText.IsEmpty()) {
			int penWidth;
			DDX_Text(pDX, IDC_PEN_WIDTH_EDIT, penWidth);
			if (figureAttribute.SetPenWidth(penWidth))
				NotifyObservers(figureAttribute);
		}
	} else {
		if (figureAttribute.IsColorValid()) {
			colorButton.SetColor(figureAttribute.GetColor());
		} else {
			colorButton.SetColor(colorButton.GetAutomaticColor());
		}

		if (figureAttribute.IsPenWidthValid()) {
			auto penWidth = figureAttribute.GetPenWidth();
			DDX_Text(pDX, IDC_PEN_WIDTH_EDIT, penWidth);
		} else {
			GetDlgItem(IDC_PEN_WIDTH_EDIT)->SetWindowText(_T(""));
		}
	}

	//if (figureAttribute.IsPenWidthValid())
	//auto penWidth = figureAttribute.GetPenWidth();
	//DDX_Text(pDX, IDC_PEN_WIDTH_EDIT, penWidth);
	//if (figureAttribute.SetPenWidth(penWidth))
	//	NotifyObservers(figureAttribute);
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
