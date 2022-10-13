#pragma once

#ifndef __AFXWIN_H__
	#error "Include 'pch.h' before including this file for PCH"
#endif

class Application : public CWinApp
{
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
