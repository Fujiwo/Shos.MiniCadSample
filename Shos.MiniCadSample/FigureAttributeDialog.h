﻿#pragma once

#include "afxdialogex.h"
#include "FigureAttribute.h"

class FigureAttributeDialog : public CDialogEx
{
	FigureAttribute* figureAttribute;
	CMFCColorButton  colorButton;
	
public:
	FigureAttributeDialog() : figureAttribute(nullptr)
	{}

	void Set(FigureAttribute& figureAttribute)
	{
		this->figureAttribute = &figureAttribute;
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

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FIGURE_ATTRIBUTE_DIALOG };
#endif
};