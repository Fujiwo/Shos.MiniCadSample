#pragma once

#include "Model.h"
#include "Command.h"
#include "MouseEventTranslator.h"

class Document : public CDocument, public Observer<Hint>, public MouseEventTranslator::Listener
{
    static const COLORREF areaColor = RGB(0xff, 0xf0, 0xe0);

    Model          model;
    CommandManager commandManager;

public:
    using iterator = Model::iterator;

    Model& GetModel() { return model; }

    static const CSize GetMinimumSize() { return Model::GetMinimumSize(); }
    static const CSize GetSize()        { return Model::GetSize()       ; }
    static const CRect GetArea()        { return Model::GetArea()       ; }

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

    void Add(Figure* figure)
    {
        model.Add(figure);
        SetModifiedFlag();
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
    
    virtual void OnInput(CPoint point) override
    {
        if (IsValid(point))
            commandManager.OnInput(point);
    }

    virtual void OnCursor(CPoint point) override
    {
        if (IsValid(point))
            commandManager.OnCursor(point);
    }

protected:
    virtual void Update(const Hint& hint) override
    {
        if (hint.type != Hint::Type::ViewOnly)
            SetModifiedFlag();
        UpdateAllViews(nullptr, 0, const_cast<Hint*>(&hint));
    }

    virtual void Serialize(CArchive& ar)
    {
        model.Serialize(ar);
    }

    virtual void DeleteContents()
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
        const size_t count = 10;
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
    
    bool IsValid(CPoint point)
    {
        return GetArea().PtInRect(point);
    }

    DECLARE_DYNCREATE(Document)
    DECLARE_MESSAGE_MAP()
};
