#pragma once
#include <afx.h>
#include <limits>
#include "Model.h"

class Command : public CObject
{
	Model* model;

protected:
	Model& GetModel() const
	{
		ASSERT_POINTER(model, Model);
		return *model;
	}

public:
	Command() : model(nullptr)
	{}

	void Set(Model& model)
	{
		this->model = &model;
	}

	virtual void Draw(CDC& dc)
	{}

	virtual void OnClick(CPoint point)
	{}

	virtual void OnMouseMove(CPoint point)
	{}

	DECLARE_DYNCREATE(Command)
};

class SelectCommand : public Command
{
	static const LONG	  searchingDistance = 100;
	static const long	  cursorPenWidth	= 3;
	static const COLORREF cursorColor		= RGB(0xff, 0x00, 0xff);

	CPoint  cursorPosition;
	bool    cursorPositionExists;
	//Figure* nearestFigure;

public:
	SelectCommand() : cursorPositionExists(false)
	{}

	virtual void Draw(CDC& dc) override
	{
		if (cursorPositionExists) {
			CPen pen(PS_SOLID, cursorPenWidth, cursorColor);
			GdiObjectSelector penSelector(dc, pen);
			
			auto deviceCursorPosition = cursorPosition;
			dc.LPtoDP(&deviceCursorPosition);

			const LONG cursorLength = 100;

			CPoint points[] = {
				{ deviceCursorPosition.x - cursorLength, deviceCursorPosition.y },
				{ deviceCursorPosition.x + cursorLength, deviceCursorPosition.y },
				{ deviceCursorPosition.x, deviceCursorPosition.y - cursorLength },
				{ deviceCursorPosition.x, deviceCursorPosition.y + cursorLength }
			};
			dc.DPtoLP(points, sizeof(points) / sizeof(CPoint));

			dc.Polyline(points	  , sizeof(points) / sizeof(CPoint) / 2);
			dc.Polyline(points + 2, sizeof(points) / sizeof(CPoint) / 2);
		}
		if (GetModel().Hilight() != nullptr)
			GetModel().Hilight()->DrawArea(dc);
	}

	virtual void OnClick(CPoint point) override
	{
		TRACE(_T("OnClick(x: %d, y: %d)\n"), point.x, point.y);

		auto nearestFigure = GetNearestFigure(point);
		if (nearestFigure == nullptr)
			GetModel().UnSelectAll();
		else
			GetModel().Select(*nearestFigure);
	}

	virtual void OnMouseMove(CPoint point)
	{
		cursorPosition		 = point;
		cursorPositionExists = true;
		auto nearestFigure = GetNearestFigure(point);
		GetModel().Hilight(nearestFigure);
	}

private:
	Figure* GetNearestFigure(CPoint point)
	{
		auto   distance			 = std::numeric_limits<long>::max();
		Figure*    nearestFigure	 = nullptr;

		CRect	   searchinRect(point, point);
		searchinRect.InflateRect(searchingDistance, searchingDistance);
		
		for (auto figure : GetModel()) {
			CRect rect;
			if (rect.IntersectRect(figure->GetArea(), searchinRect)) {
				auto figureDistance = figure->GetDistanceFrom(point);
				if (figureDistance < distance) {
					distance	  = figureDistance;
					nearestFigure = figure;
				}
			}
		}
		return nearestFigure;
	}
	
	DECLARE_DYNCREATE(SelectCommand)
};

class CommandManager
{
	Model&	  model;
	Command*  currentCommand;
	
public:
	CommandManager(Model& model) : model(model), currentCommand(nullptr)
	{
		SetCommand(new SelectCommand());
	}

	virtual ~CommandManager()
	{
		delete currentCommand;
	}
	
	void SetCommand(Command* command)
	{
		delete currentCommand;
		currentCommand = command;
		currentCommand->Set(model);
	}
	
	void Draw(CDC& dc)
	{
		if (GetCurrentCommand() != nullptr)
			GetCurrentCommand()->Draw(dc);
	}

	void OnClick(CPoint point)
	{
		if (GetCurrentCommand() != nullptr)
			GetCurrentCommand()->OnClick(point);
	}

	void OnMouseMove(CPoint point)
	{
		if (GetCurrentCommand() != nullptr)
			GetCurrentCommand()->OnMouseMove(point);
	}

private:
	Command* GetCurrentCommand() const
	{
		if (currentCommand == nullptr)
			return nullptr;

		ASSERT_VALID(currentCommand);
		return currentCommand;
	}
};