﻿#include "pch.h"
#include "framework.h"
#include "Document.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(Document, CDocument)

BEGIN_MESSAGE_MAP(Document, CDocument)
	ON_COMMAND(ID_FIGURE_DOT, OnFigureDot)
	ON_UPDATE_COMMAND_UI(ID_FIGURE_DOT, OnUpdateFigureDot)
	ON_COMMAND(ID_FIGURE_LINE, OnFigureLine)
	ON_UPDATE_COMMAND_UI(ID_FIGURE_LINE, OnUpdateFigureLine)
	ON_COMMAND(ID_FIGURE_RECTANGLE, OnFigureRectangle)
	ON_UPDATE_COMMAND_UI(ID_FIGURE_RECTANGLE, OnUpdateFigureRectangle)
	ON_COMMAND(ID_FIGURE_ELLIPSE, OnFigureEllipse)
	ON_UPDATE_COMMAND_UI(ID_FIGURE_ELLIPSE, OnUpdateFigureEllipse)
	ON_COMMAND(ID_FIGURE_RANDOM, OnFigureRandom)
	ON_COMMAND(ID_FIGURE_SELECT, OnFigureSelect)
	ON_UPDATE_COMMAND_UI(ID_FIGURE_SELECT, OnUpdateFigureSelect)
END_MESSAGE_MAP()
