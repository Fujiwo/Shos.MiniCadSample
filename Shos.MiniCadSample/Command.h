#pragma once
#include <afx.h>
#include <limits>
#include "Common.h"
#include "Model.h"

class Cursor
{
public:
	class MessageHolder
	{
	public:
		virtual CString GetMessage() const
		{
			return CString();
		}
	};

private:
	static const long	  cursorPenWidth = 3;
	static const COLORREF cursorColor = RGB(0xff, 0x00, 0xff);

	CPoint  cursorPosition;
	bool    cursorPositionExists;

public:
	Cursor() : cursorPositionExists(false)
	{}

	void Draw(CDC& dc, const MessageHolder& messageHolder)
	{
		if (!cursorPositionExists)
			return;

		DrawCursor(dc, cursorPosition);
		DrawMessage(dc, cursorPosition, messageHolder);
	}

	void SetCursorPosition(CPoint point)
	{
		cursorPosition		 = point;
		cursorPositionExists = true;
	}

private:
	static void DrawCursor(CDC& dc, CPoint cursorPosition)
	{
		CPen pen(PS_SOLID, cursorPenWidth, cursorColor);
		GdiObjectSelector penSelector(dc, pen);

		const LONG cursorLength = 100;

		auto deviceCursorPosition = cursorPosition;
		dc.LPtoDP(&deviceCursorPosition);

		CPoint points[] = {
			{ deviceCursorPosition.x - cursorLength, deviceCursorPosition.y },
			{ deviceCursorPosition.x + cursorLength, deviceCursorPosition.y },
			{ deviceCursorPosition.x, deviceCursorPosition.y - cursorLength },
			{ deviceCursorPosition.x, deviceCursorPosition.y + cursorLength }
		};
		dc.DPtoLP(points, sizeof(points) / sizeof(CPoint));

		dc.Polyline(points, sizeof(points) / sizeof(CPoint) / 2);
		dc.Polyline(points + 2, sizeof(points) / sizeof(CPoint) / 2);
	}

	static void DrawMessage(CDC& dc, CPoint cursorPosition, const MessageHolder& messageHolder)
	{
		auto dcId = dc.SaveDC();

		dc.SetTextColor(cursorColor);
		dc.SetBkColor(Common::areaColor);
		dc.SetBkMode(TRANSPARENT);

		dc.TextOut(cursorPosition.x, cursorPosition.y, messageHolder.GetMessage());

		dc.RestoreDC(dcId);
	}
};

class Command : public CObject, public Cursor::MessageHolder
{
	Model* model;
	Cursor cursor;

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

	void Draw(CDC& dc)
	{
		cursor.Draw(dc, *this);
		OnDraw(dc);
	}

	virtual void OnClick(CPoint point)
	{}

	void OnMouseMove(CPoint point)
	{
		cursor.SetCursorPosition(point);
		OnCursotMove(point);
	}

protected:
	virtual void OnDraw(CDC& dc)
	{}

	virtual void OnCursotMove(CPoint point)
	{}

	DECLARE_DYNCREATE(Command)
};

class SelectCommand : public Command
{
	static const LONG searchingDistance = 100;

	bool			  hasDistanceToFigure;
	long			  distanceToFigure;

public:
	SelectCommand() : hasDistanceToFigure(false), distanceToFigure(0L)
	{}

	virtual void OnDraw(CDC& dc) override
	{
		if (GetModel().Hilight() != nullptr)
			GetModel().Hilight()->DrawArea(dc);
	}

	virtual void OnClick(CPoint point) override
	{
		TRACE(_T("OnClick(x: %d, y: %d)\n"), point.x, point.y);

		auto nearestFigure = GetNearestFigure(point);
		if (nearestFigure == nullptr) {
			GetModel().UnSelectAll();
			GetModel().ResetSelectedFigureAttribute();
		} else {
			GetModel().Select(*nearestFigure);
			GetModel().SetSelectedFigureAttribute(nearestFigure->Attribute());
		}
	}

	virtual void OnCursotMove(CPoint point) override
	{
		auto nearestFigure = GetNearestFigure(point, &distanceToFigure);
		if (nearestFigure != nullptr)
			hasDistanceToFigure = true;
		GetModel().Hilight(nearestFigure);
	}

	virtual CString GetMessage() const override
	{
		CString message;
		if (hasDistanceToFigure)
			message.Format(_T("distance: %d"), distanceToFigure);
		return message;
	}

private:
	Figure* GetNearestFigure(CPoint point, long* distance = nullptr)
	{
		auto   minimumDistance = std::numeric_limits<long>::max();
		Figure*    nearestFigure   = nullptr;

		CRect	   searchinRect(point, point);
		searchinRect.InflateRect(searchingDistance, searchingDistance);
		
		for (auto figure : GetModel()) {
			CRect rect;
			if (rect.IntersectRect(figure->GetArea(), searchinRect)) {
				auto figureDistance = figure->GetDistanceFrom(point);
				if (figureDistance < minimumDistance) {
					minimumDistance	= figureDistance;
					nearestFigure   = figure;
				}
			}
		}
		if (distance != nullptr)
			*distance = minimumDistance;
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