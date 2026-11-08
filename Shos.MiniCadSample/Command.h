#pragma once
// Commands: the tools that turn user input (in logical coordinates) into model changes.
// A Command handles clicks and drags and draws its own rubber band; CommandManager holds the current one.

#include <afx.h>
#include <iterator>
#include <memory>
#include <vector>
#include "Geometry.h"
#include "Model.h"
#include "MouseEventTranslator.h"

namespace Shos::MiniCadSample::Controllers {

using namespace Models;
using namespace Utilities;

// Draws a cross-hair at the last cursor position, with a one-line text next to it.
// All positions are logical coordinates; the cross-hair size and the text size are fixed in device pixels.
class Cursor
{
public:
    // Supplies the text shown next to the cursor. Formatted as "[name] message", or just the message if there is no name.
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
                textline.Format(_T("[%s] %s"), GetName().GetString(), GetMessage().GetString());
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
    // Half length of each cross-hair line, in device pixels.
    static constexpr LONG     cursorLength    = 120;
    static constexpr long     cursorPenWidth  = 3;
    static constexpr COLORREF cursorColor     = RGB(0xff, 0x00, 0xff);
    static constexpr int      messageTextSize = cursorLength / 3;
    static constexpr int      messageMargin   = 10;

    // Last cursor position (logical). Nothing is drawn until the first SetCursorPosition.
    CPoint  cursorPosition;
    bool    cursorPositionExists = false;

public:
    // Draws the cross-hair and the message at the cursor position. Does nothing if the position is not known yet.
    void Draw(CDC& dc, const MessageHolder& messageHolder)
    {
        if (!cursorPositionExists)
            return;

        DrawCursor(dc, cursorPosition);
        DrawMessage(dc, cursorPosition, messageHolder);
    }

    void SetCursorPosition(CPoint point)
    {
        cursorPosition       = point;
        cursorPositionExists = true;
    }

private:
    // The line length is a device size: convert to device coordinates, offset there, and convert back, so the
    // cross-hair has the same size on screen at any zoom.
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
        constexpr auto pointCount = static_cast<int>(std::ssize(points));
        dc.DPtoLP(points, pointCount);

        dc.Polyline(points, pointCount / 2);
        dc.Polyline(points + pointCount / 2, pointCount / 2);
    }

    // Draws the message above and to the right of the cursor. Text size and margin are converted from device to
    // logical units so they stay constant on screen. The DC state is restored on return.
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
        // The other members are zero.
        LOGFONT logFont{
            .lfHeight         = textSize,
            .lfCharSet        = DEFAULT_CHARSET,
            .lfOutPrecision   = OUT_DEFAULT_PRECIS,
            .lfClipPrecision  = CLIP_DEFAULT_PRECIS,
            .lfQuality        = DEFAULT_QUALITY,
            .lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE,
            .lfFaceName       = _T("Arial")
        };

        font.CreateFontIndirect(&logFont);
    }
};

// Base class of all tools. It receives input from MouseEventTranslator (through Document and CommandManager)
// in logical coordinates. Subclasses override the protected hooks (OnInput, OnCursorMove, OnDraw) and
// GetMaximumCount, which is the number of points the tool needs (2 means it can also be driven by dragging).
// The command is owned by CommandManager and does not own the Model.
class Command : public CObject, public Cursor::MessageHolder, public MouseEventTranslator::Listener
{
    // The model to operate on. Not owned; set by CommandManager::SetCommand.
    Model* model = nullptr;
    Cursor cursor;

protected:
    Model& GetModel() const
    {
        ASSERT_POINTER(model, Model);
        return *model;
    }
    
public:
    // Binds the command to the model. Must be called before any input is delivered.
    void Set(Model& newModel)
    {
        model = &newModel;
    }

    // Draws the cursor and then the command's own rubber band. Called for the upper (per-paint) layer of the view.
    void Draw(CDC& dc)
    {
        cursor.Draw(dc, *this);
        OnDraw(dc);
    }

    // By default a drag is treated as two clicks (the start and the end) for a two-point tool with the left button,
    // and it moves the cursor while dragging. Otherwise it is ignored (e.g. the right button is for panning).
    virtual void OnDragStart(UINT keys, CPoint point) override
    {
        if (IsDraggable(keys))
            OnClick(point);
    }

    virtual void OnDragEnd(UINT keys, CPoint point) override
    {
        if (IsDraggable(keys))
            OnClick(point);
    }

    virtual void OnDragging(UINT keys, CPoint point) override
    {
        if (IsDraggable(keys))
            OnCursor(point);
    }

    virtual void OnClick(CPoint point) override
    {
        OnInput(point);
    }

    virtual void OnCursor(CPoint point) override
    {
        cursor.SetCursorPosition(point);
        OnCursorMove(point);
    }

protected:
    // Hook: draws the rubber band (the state being input). The cursor is drawn by Draw.
    virtual void OnDraw(CDC& /* dc */)
    {}

    // Hook: a point was entered (a click, or the start or end of a drag).
    virtual void OnInput(CPoint /* point */)
    {}

    // Hook: the cursor moved (with no button pressed, or while dragging).
    virtual void OnCursorMove(CPoint /* point */)
    {}

    // Returns true if a drag with these keys should be handled: the left button, and a tool that takes two points.
    virtual bool IsDraggable(UINT keys)
    {
        return (keys & MK_LBUTTON) != 0L && GetMaximumCount() == 2;
    }

    // Number of points the tool takes to complete one operation. 0 means unknown (not draggable).
    virtual size_t GetMaximumCount() const
    {
        return 0UL;
    }

    DECLARE_DYNCREATE(Command)
};

// Selects figures: a click selects the nearest figure (or clears the selection if none is near), and a drag
// selects the figures in the dragged rectangle. While the cursor moves, the nearest figure is highlighted
// and its distance is shown.
class SelectCommand : public Command
{
    // Figures farther than this (logical units) from the cursor are not candidates for highlight or selection.
    static constexpr LONG     searchingDistance = 100;

    // Colors of the selection rectangle. It is drawn with XOR (R2_XORPEN), so these are combined with the background.
    static constexpr COLORREF areaPenColor      = RGB(0x40, 0x60, 0x80);
    static constexpr COLORREF areaBrushColor    = RGB(0x20, 0x30, 0x40);

    // Distance from the cursor to the highlighted figure, shown in the message. Valid only if hasDistanceToFigure.
    bool              hasDistanceToFigure = false;
    long              distanceToFigure    = 0L;

    // The drag start point, and the normalized rectangle being dragged. area is drawn and used only while isAreaValid.
    CPoint            areaTopLeft;
    CRect             area;
    bool              isAreaValid         = false;

protected:
    virtual void OnDraw(CDC& dc) override
    {
        if (GetModel().Highlight() != nullptr)
            GetModel().Highlight()->DrawArea(dc);

        DrawArea(dc);
    }

    // Selects the figure nearest to the point, or clears the selection if no figure is near.
    virtual void OnInput(CPoint point) override
    {
        TRACE(_T("OnClick(x: %d, y: %d)\n"), point.x, point.y);

        auto nearestFigure = GetNearestFigure(point);
        if (nearestFigure == nullptr)
            GetModel().UnSelectAll();
        else
            GetModel().Select(*nearestFigure);
    }

    // Highlights the nearest figure (or none) and remembers its distance for the message.
    virtual void OnCursorMove(CPoint point) override
    {
        auto nearestFigure = GetNearestFigure(point, &distanceToFigure);
        hasDistanceToFigure       = (nearestFigure != nullptr);
        GetModel().Highlight(nearestFigure);
    }
    
    virtual void OnDragStart(UINT keys, CPoint point) override
    {
        if (IsDraggable(keys)) {
            isAreaValid = false;
            areaTopLeft = point;
        }
    }

    virtual void OnDragging(UINT keys, CPoint point) override
    {
        if (IsDraggable(keys)) {
            SetArea(point);
            isAreaValid = true;
        }
    }
    
    virtual void OnDraggingAbort() override
    {
        isAreaValid = false;
    }

    // Selects the figures in the dragged rectangle. Ignored if the drag was aborted or never produced a rectangle.
    virtual void OnDragEnd(UINT keys, CPoint point) override
    {
        if (!isAreaValid)
            return;
        
        if (IsDraggable(keys)) {
            isAreaValid = false;
            SetArea(point);
            GetModel().Select(area);
        }
    }

    virtual CString GetName() const override
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

    virtual size_t GetMaximumCount() const override
    {
        return 2UL;
    }

private:
    // Returns the figure nearest to the point within searchingDistance, or nullptr. If distance is not null,
    // it receives the distance (0 if there is no figure). The model's bounding rectangles are used to skip far figures.
    Figure* GetNearestFigure(CPoint point, long* distance = nullptr)
    {
        auto       minimumDistance = Geometry::maximumDistance;
        Figure*    nearestFigure   = nullptr;

        CRect      searchingRect(point, point);
        searchingRect.InflateRect(searchingDistance, searchingDistance);
        
        for (auto figure : GetModel()) {
            CRect rect;
            if (rect.IntersectRect(figure->GetArea(), searchingRect)) {
                auto figureDistance = figure->GetDistanceFrom(point);
                if (figureDistance < minimumDistance) {
                    minimumDistance = figureDistance;
                    nearestFigure   = figure;
                }
            }
        }
        if (distance != nullptr)
            *distance = nearestFigure == nullptr ? 0L : minimumDistance;
        return nearestFigure;
    }

    // Updates the dragged rectangle from the start point to the point.
    void SetArea(CPoint point)
    {
        area = CRect(areaTopLeft, point);
        area.NormalizeRect();
    }

    // Draws the selection rectangle (XOR) while a drag is in progress.
    void DrawArea(CDC& dc) const
    {
        if (isAreaValid) {
            CPen pen(PS_SOLID, 0, areaPenColor);
            GdiObjectSelector penSelector(dc, pen);

            CBrush brush(areaBrushColor);
            GdiObjectSelector brushSelector(dc, brush);

            auto rop = dc.SetROP2(R2_XORPEN);
            dc.Rectangle(&area);
            dc.SetROP2(rop);
        }
    }

    DECLARE_DYNCREATE(SelectCommand)
};

// Base class of the tools that add one figure. It collects points (clicks, or drag start and end) until
// GetMaximumCount points are entered, then creates the figure and adds it to the Model (one undo step).
// While input is in progress, a preview of the figure up to the cursor is drawn as the rubber band.
class AddFigureCommand : public Command
{
    CPoint              cursorPosition;
    // The points entered so far for the figure being created (logical). Cleared when the figure is added.
    std::vector<CPoint> points;

protected:
    // Draws a preview of the figure, using the current figure attribute, once at least one point is entered.
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

    // Adds the point if Input accepts it. When the last point is entered, the figure is created and handed to the
    // Model (which takes the ownership), and the input starts over.
    virtual void OnInput(CPoint point) override
    {
        if (Input(GetCount(), point))
            points.push_back(point);
        else
            return;

        if (GetCount() == GetMaximumCount()) {
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

    // Returns a temporary figure for the preview, from the entered points and the given cursor point. It is owned
    // by the command and must not be passed to the Model.
    virtual Figure* GetFigure(CPoint /* point */) = 0;
    // Creates the final figure (allocated with new) from the entered points. The caller passes it to the Model.
    virtual Figure* CreateFigure() = 0;

    // Hook: returns whether the point is acceptable as the next point (count points are already entered).
    virtual bool Input(size_t /* count */, CPoint /* point */)
    {
        return true;
    }
};

// Adds a dot: one click.
class DotCommand : public AddFigureCommand
{
    DotFigure figure;

protected:
    virtual Figure* GetFigure(CPoint point) override
    {
        figure = DotFigure(point);
        return &figure;
    }

    virtual Figure* CreateFigure() override
    {
        return new DotFigure(GetPoint(0));
    }

    virtual size_t GetMaximumCount() const override
    {
        return 1;
    }

    virtual CString GetName() const override
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

// Adds a line: two points (start and end).
class LineCommand : public AddFigureCommand
{
    LineFigure figure;

protected:
    virtual Figure* GetFigure(CPoint point) override
    {
        figure = LineFigure(GetPoint(0), point);
        return &figure;
    }

    virtual Figure* CreateFigure() override
    {
        return new LineFigure(GetPoint(0), GetPoint(1));
    }

    virtual size_t GetMaximumCount() const override
    {
        return 2;
    }

    virtual CString GetName() const override
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

// Common part of the tools that take two opposite corners (rectangle and ellipse), including the size message.
class RectangleBaseCommand : public AddFigureCommand
{
protected:
    virtual size_t GetMaximumCount() const override
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

// Adds a rectangle from two opposite corners.
class RectangleCommand : public RectangleBaseCommand
{
    RectangleFigure figure;

protected:
    virtual Figure* GetFigure(CPoint point) override
    {
        figure = RectangleFigure(CRect(GetPoint(0), point));
        return &figure;
    }

    virtual Figure* CreateFigure() override
    {
        return new RectangleFigure(CRect(GetPoint(0), GetPoint(1)));
    }

    virtual CString GetName() const override
    {
        return _T("Rectangle");
    }

    DECLARE_DYNCREATE(RectangleCommand)
};

// Adds an ellipse inscribed in the rectangle of two opposite corners.
class EllipseCommand : public RectangleBaseCommand
{
    EllipseFigure figure;

protected:
    virtual Figure* GetFigure(CPoint point) override
    {
        figure = EllipseFigure(CRect(GetPoint(0), point));
        return &figure;
    }

    virtual Figure* CreateFigure() override
    {
        return new EllipseFigure(CRect(GetPoint(0), GetPoint(1)));
    }

    virtual CString GetName() const override
    {
        return _T("Ellipse");
    }

    DECLARE_DYNCREATE(EllipseCommand)
};

// Owns the current Command and forwards every input event to it. Switching tools means SetCommand.
// It starts with SelectCommand.
class CommandManager : public MouseEventTranslator::Listener
{
    // The model passed to each command. Not owned.
    Model&                   model;
    std::unique_ptr<Command> currentCommand;
    
public:
    explicit CommandManager(Model& model) : model(model)
    {
        SetCommand(new SelectCommand());
    }

    // Returns true if the current command is of the given class (or a subclass). Used for the check marks of the tool menu.
    bool IsRunning(CRuntimeClass* commandRuntimeClass) const
    {
        return currentCommand != nullptr && currentCommand->IsKindOf(commandRuntimeClass);
    }
    
    // Takes the ownership of command (allocated with new).
    void SetCommand(Command* command)
    {
        currentCommand.reset(command);
        currentCommand->Set(model);
    }
    
    // Draws the current command's cursor and rubber band.
    void Draw(CDC& dc)
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->Draw(dc);
    }

    virtual void OnClick(CPoint point) override
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->OnClick(point);
    }

    virtual void OnCursor(CPoint point) override
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->OnCursor(point);
    }

    virtual void OnDragStart(UINT keys, CPoint point) override
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->OnDragStart(keys, point);
    }

    virtual void OnDragging(UINT keys, CPoint point) override
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->OnDragging(keys, point);
    }

    virtual void OnDraggingAbort() override
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->OnDraggingAbort();
    }

    virtual void OnDragEnd(UINT keys, CPoint point) override
    {
        if (GetCurrentCommand() != nullptr)
            GetCurrentCommand()->OnDragEnd(keys, point);
    }

private:
    // Returns the current command (nullptr only if none was set), after checking that it is a valid object.
    Command* GetCurrentCommand() const
    {
        if (currentCommand == nullptr)
            return nullptr;

        ASSERT_VALID(currentCommand.get());
        return currentCommand.get();
    }
};

} // namespace Shos::MiniCadSample::Controllers
