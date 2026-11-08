#pragma once

#include "Model.h"
#include "Command.h"
#include "MouseEventTranslator.h"

namespace Shos::MiniCadSample::Controllers {

using namespace Models;
using namespace Utilities;

class Document : public CDocument, public Observer<Hint>, public MouseEventTranslator::Listener
{
    static constexpr COLORREF areaColor = RGB(0xff, 0xf0, 0xe0);

    Model          model;
    CommandManager commandManager;
    bool           isDragging = false; // Whether the current drag started in the area (and so was passed to the command).

public:
    using iterator = Model::iterator;

    Model& GetModel() { return model; }

    static CSize GetMinimumSize() { return Model::GetMinimumSize(); }
    static CSize GetSize()        { return Model::GetSize()       ; }
    static CRect GetArea()        { return Model::GetArea()       ; }

    FigureAttribute& GetCurrentFigureAttribute()
    {
        return model.GetCurrentFigureAttribute();
    }

    Document() : commandManager(model)
    {
        model.AddObserver(*this);
    }

    iterator begin() const
    {
        return model.begin();
    }

    iterator end() const
    {
        return model.end();
    }

    void Draw(CDC& dc) const
    {
        DrawArea(dc);
        for (auto figure : *this)
            figure->Draw(dc);
    }

    void DrawArea(CDC& dc) const
    {
        dc.FillSolidRect(GetArea(), areaColor);
    }

    void DrawCommand(CDC& dc)
    {
        commandManager.Draw(dc);
    }

    void RemoveSelectedFigures()
    {
        model.RemoveSelectedFigures();
    }
    
    virtual void OnClick(CPoint point) override
    {
        if (IsValid(point))
            commandManager.OnClick(point);
    }

    virtual void OnCursor(CPoint point) override
    {
        if (IsValid(point))
            commandManager.OnCursor(point);
    }

    // A drag is passed to the command only if it starts in the area. Once started, it goes on outside the area,
    // with the point clamped to the area, so that the command always gets the end of the drag
    // (e.g. a selection rectangle stops at the edge of the area, and is not left on the screen).
    virtual void OnDragStart(UINT keys, CPoint point) override
    {
        isDragging = IsValid(point);
        if (isDragging)
            commandManager.OnDragStart(keys, point);
    }

    virtual void OnDragging(UINT keys, CPoint point) override
    {
        if (isDragging)
            commandManager.OnDragging(keys, Geometry::Clamp(point, GetArea()));
    }

    virtual void OnDraggingAbort() override
    {
        isDragging = false;
        commandManager.OnDraggingAbort();
    }

    virtual void OnDragEnd(UINT keys, CPoint point) override
    {
        if (isDragging) {
            isDragging = false;
            commandManager.OnDragEnd(keys, Geometry::Clamp(point, GetArea()));
        }
    }

protected:
    virtual void Update(const Hint& hint) override
    {
        if (hint.type != Hint::Type::ViewOnly)
            SetModifiedFlag();
        UpdateAllViews(nullptr, 0, const_cast<Hint*>(&hint));
    }

    virtual void Serialize(CArchive& ar) override
    {
        model.Serialize(ar);
    }

public:
    // Adds the figures on the clipboard (written by Model::StoreFigures) as one undo step. See Model::Paste.
    void Paste(CArchive& ar)
    {
        model.Paste(ar);
    }

protected:

    virtual void DeleteContents() override
    {
        model.Reset();
        CDocument::DeleteContents();
    }

    afx_msg void OnEditUndo()
    {
        model.Undo();
    }
    
    afx_msg void OnUpdateEditUndo(CCmdUI* cmdUI)
    {
        cmdUI->Enable(model.CanUndo() ? 1 : 0);
    }

    afx_msg void OnEditRedo()
    {
        model.Redo();
    }

    afx_msg void OnUpdateEditRedo(CCmdUI* cmdUI)
    {
        cmdUI->Enable(model.CanRedo() ? 1 : 0);
    }
    
    afx_msg void OnEditDelete()
    {
        model.RemoveSelectedFigures();
    }

    afx_msg void OnUpdateEditDelete(CCmdUI* cmdUI)
    {
        cmdUI->Enable(model.CanRemoveSelectedFigures() ? 1 : 0);
    }

    afx_msg void OnFigureDot()
    {
        SetCommand(new DotCommand());
    }

    afx_msg void OnUpdateFigureDot(CCmdUI* cmdUI)
    {
        cmdUI->SetCheck(commandManager.IsRunning(RUNTIME_CLASS(DotCommand)) ? 1 : 0);
    }

    afx_msg void OnFigureLine()
    {
        SetCommand(new LineCommand());
    }

    afx_msg void OnUpdateFigureLine(CCmdUI* cmdUI)
    {
        cmdUI->SetCheck(commandManager.IsRunning(RUNTIME_CLASS(LineCommand)) ? 1 : 0);
    }

    afx_msg void OnFigureRectangle()
    {
        SetCommand(new RectangleCommand());
    }

    afx_msg void OnUpdateFigureRectangle(CCmdUI* cmdUI)
    {
        cmdUI->SetCheck(commandManager.IsRunning(RUNTIME_CLASS(RectangleCommand)) ? 1 : 0);
    }

    afx_msg void OnFigureEllipse()
    {
        SetCommand(new EllipseCommand());
    }

    afx_msg void OnUpdateFigureEllipse(CCmdUI* cmdUI)
    {
        cmdUI->SetCheck(commandManager.IsRunning(RUNTIME_CLASS(EllipseCommand)) ? 1 : 0);
    }

    afx_msg void OnFigureRandom()
    {
        constexpr size_t count = 10;
        model.AddDummyData(count);
    }

    afx_msg void OnFigureSelect()
    {
        SetCommand(new SelectCommand());
    }

    afx_msg void OnUpdateFigureSelect(CCmdUI* cmdUI)
    {
        cmdUI->SetCheck(commandManager.IsRunning(RUNTIME_CLASS(SelectCommand)) ? 1 : 0);
    }

private:
    void SetCommand(Command* command)
    {
        commandManager.SetCommand(command);
        UpdateAllViews(nullptr);
    }
    
    static bool IsValid(CPoint point)
    {
        return GetArea().PtInRect(point);
    }

    DECLARE_DYNCREATE(Document)
    DECLARE_MESSAGE_MAP()
};

} // namespace Shos::MiniCadSample::Controllers
