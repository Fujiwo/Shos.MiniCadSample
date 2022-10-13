#include "pch.h"
#include "framework.h"
#include "Document.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(Document, CDocument)

BEGIN_MESSAGE_MAP(Document, CDocument)
	ON_COMMAND(ID_FIGURE_RANDOM, OnFigureRandom)
END_MESSAGE_MAP()
