#pragma once

// MFC headers used by every file of the application (included through pch.h).

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN // Excludes rarely used parts of the Windows headers.
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // Makes some CString constructors explicit.

#define _AFX_ALL_WARNINGS // Keeps MFC's common warnings enabled.

#include <afxwin.h>
#include <afxext.h>

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>










