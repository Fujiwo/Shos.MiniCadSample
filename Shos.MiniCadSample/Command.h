#pragma once
#include <afx.h>
#include "Geometry.h"
#include "Model.h"

class Cursor
{
public:
	class MessageHolder
	{
	public:
		virtual CString GetTextline() const
		{
			CString textline;
			auto name = GetName();
			if (name.IsEmpty())
				textline.Format(_T("%s"), GetMessage().GetString());
			else
				textline.Format(_T("[%s] %s"), GetName(), GetMessage().GetString());
			return textline;
		}

	protected:
		virtual CString GetName() const
		{
			return CString();
		}

		virtual CString GetMessage() const
		{
			return CString();
		}
	};

private:
	static const LONG	  cursorLength    = 120;
	static const long	  cursorPenWidth  = 3;
	static const COLORREF cursorColor     = RGB(0xff, 0x00, 0xff);
	static const int      messageTextSize = cursorLength / 3;
	static const int      messageMargin	  = 10;

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

		auto logicalMessageTextSize = Geometry::DPtoLP(dc, messageTextSize);
		
		CFont   font;
		CreateMessageFont(font, logicalMessageTextSize);
		dc.SelectObject(&font);

		dc.SetTextColor(cursorColor);
		dc.SetBkMode(TRANSPARENT);

		auto logicalMessageMargin = Geometry::DPtoLP(dc, messageMargin);
		dc.TextOut(cursorPosition.x + logicalMessageMargin, cursorPosition.y - logicalMessageTextSize - logicalMessageMargin, messageHolder.GetTextline());

		dc.RestoreDC(dcId);
	}

	static void CreateMessageFont(CFont& font, int textSize)
	{
		LOGFONT logFont;
		::ZeroMemory(&logFont, sizeof(logFont));
		logFont.lfHeight		 = textSize;
		logFont.lfCharSet		 = DEFAULT_CHARSET;
		logFont.lfOutPrecision	 = OUT_DEFAULT_PRECIS;
		logFont.lfClipPrecision	 = CLIP_DEFAULT_PRECIS;
		logFont.lfQuality		 = DEFAULT_QUALITY;
		logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
		lstrcpy(logFont.lfFaceName, _T("Arial"));

		font.CreateFontIndirect(&logFont);
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
		OnCursorMove(point);
	}

protected:
	virtual void OnDraw(CDC& dc)
	{}

	virtual void OnCursorMove(CPoint point)
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

protected:
	virtual void OnDraw(CDC& dc) override
	{
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

	virtual void OnCursorMove(CPoint point) override
	{
		auto nearestFigure = GetNearestFigure(point, &distanceToFigure);
		hasDistanceToFigure		  = (nearestFigure != nullptr);
		GetModel().Hilight(nearestFigure);
	}

	virtual CString GetName() const
	{
		return _T("Select");
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
		auto	   minimumDistance = Geometry::maximumDistance;
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
			*distance = nearestFigure == nullptr ? 0L : minimumDistance;
		return nearestFigure;
	}
	
	DECLARE_DYNCREATE(SelectCommand)
};

class AddFigureCommand : public Command
{
	CPoint				cursorPosition;
	std::vector<CPoint> points;

public:
	AddFigureCommand()
	{}

protected:
	virtual void OnDraw(CDC& dc) override
	{
		if (GetCount() > 0) {
			auto figure = GetFigure(cursorPosition);
			if (figure == nullptr)
				return;
			figure->Attribute() = GetModel().GetCurrentFigureAttribute();
			figure->Draw(dc);
		}
	}

	virtual void OnClick(CPoint point) override
	{
		if (Input(GetCount(), point))
			points.push_back(point);
		else
			return;

		if (GetCount() == GetMaxCount()) {
			auto figure = CreateFigure();
			ASSERT_VALID(figure);
			GetModel().Add(figure);
			points.clear();
		}
	}

	virtual void OnCursorMove(CPoint point) override
	{
		cursorPosition = point;
	}

protected:
	size_t GetCount() const
	{
		return points.size();
	}

	CPoint GetPoint(size_t index) const
	{
		return points[index];
	}

	CPoint GetCursorPosition() const
	{
		return cursorPosition;
	}

	virtual Figure* GetFigure(CPoint cursorPosition) = 0;
	virtual Figure* CreateFigure() = 0;

	virtual size_t GetMaxCount() const
	{
		return 2;
	}

	virtual bool Input(size_t count, CPoint point)
	{
		return true;
	}
};

class DotCommand : public AddFigureCommand
{
	DotFigure figure;

protected:
	virtual Figure* GetFigure(CPoint cursorPosition) override
	{
		figure = DotFigure(cursorPosition);
		return &figure;
	}

	virtual Figure* CreateFigure() override
	{
		return new DotFigure(GetPoint(0));
	}

	virtual size_t GetMaxCount() const
	{
		return 1;
	}

	virtual CString GetName() const
	{
		return _T("Dot");
	}

	virtual CString GetMessage() const override
	{
		CString message;
		message.Format(_T("Click the point. (x: %d, y: %d)"), GetCursorPosition().x, GetCursorPosition().y);
		return message;
	}

	DECLARE_DYNCREATE(DotCommand)
};

class LineCommand : public AddFigureCommand
{
	LineFigure figure;

protected:
	virtual Figure* GetFigure(CPoint cursorPosition) override
	{
		figure = LineFigure(GetPoint(0), cursorPosition);
		return &figure;
	}

	virtual Figure* CreateFigure() override
	{
		return new LineFigure(GetPoint(0), GetPoint(1));
	}

	virtual size_t GetMaxCount() const
	{
		return 2;
	}

	virtual CString GetName() const
	{
		return _T("Line");
	}

	virtual CString GetMessage() const override
	{
		CString message;
		if (GetCount() > 0)
			message.Format(_T("Click end point. (Length: %d)"), Geometry::GetDistance(GetPoint(0), GetCursorPosition()));
		else
			message = _T("Click start point.");
		return message;
	}

	DECLARE_DYNCREATE(LineCommand)
};

class RectangleBaseCommand : public AddFigureCommand
{
protected:
	virtual size_t GetMaxCount() const
	{
		return 2;
	}

	virtual CString GetMessage() const override
	{
		CString message;
		if (GetCount() > 0) {
			CRect rect(GetPoint(0), GetCursorPosition());
			rect.NormalizeRect();
			message.Format(_T("Click another point. (Width: %d, Height: %d)"), rect.Width(), rect.Height());
		} else {
			message = _T("Click point.");
		}
		return message;
	}
};

class RectangleCommand : public RectangleBaseCommand
{
	RectangleFigure figure;

protected:
	virtual Figure* GetFigure(CPoint cursorPosition) override
	{
		figure = RectangleFigure(CRect(GetPoint(0), cursorPosition));
		return &figure;
	}

	virtual Figure* CreateFigure() override
	{
		return new RectangleFigure(CRect(GetPoint(0), GetPoint(1)));
	}

	virtual CString GetName() const
	{
		return _T("Rectangle");
	}

	DECLARE_DYNCREATE(RectangleCommand)
};

class EllipseCommand : public RectangleBaseCommand
{
	EllipseFigure figure;

protected:
	virtual Figure* GetFigure(CPoint cursorPosition) override
	{
		figure = EllipseFigure(CRect(GetPoint(0), cursorPosition));
		return &figure;
	}

	virtual Figure* CreateFigure() override
	{
		return new EllipseFigure(CRect(GetPoint(0), GetPoint(1)));
	}

	virtual CString GetName() const
	{
		return _T("Ellipse");
	}

	DECLARE_DYNCREATE(EllipseCommand)
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

	bool IsRunning(CRuntimeClass* commandRuntimeClass) const
	{
		return currentCommand == nullptr ? false : currentCommand->IsKindOf(commandRuntimeClass);
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