// Application startup: the document template that ties Document, MainFrame and View together.
#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "Application.h"
#include "MainFrame.h"
#include "Document.h"
#include "View.h"
#include "resource.h"

namespace Shos::MiniCadSample {

using namespace Controllers;
using namespace Views;

BEGIN_MESSAGE_MAP(Application, CWinApp)
    // Standard File > New / Open (the framework provides the handlers).
    ON_COMMAND(ID_FILE_NEW, OnFileNew)
    ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
END_MESSAGE_MAP()

BOOL Application::InitInstance()
{
    CWinApp::InitInstance();

    // SDI: one document at a time; the template owns the classes created by the framework. The framework deletes it.
    auto pDocTemplate = new CSingleDocTemplate(IDR_MAINFRAME, RUNTIME_CLASS(Document), RUNTIME_CLASS(MainFrame), RUNTIME_CLASS(View));
    if (!pDocTemplate)
        return FALSE;
    AddDocTemplate(pDocTemplate);

    // Handles the command line (the default is File > New).
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    if (!ProcessShellCommand(cmdInfo))
        return FALSE;

    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->UpdateWindow();
    return TRUE;
}

// The one and only application object.
Application theApp;

} // namespace Shos::MiniCadSample
