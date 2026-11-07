#pragma once

#include <afx.h>
#include "Observer.h"
#include "Figure.h"
#include "Application.h"
#include "undo_redo_vector.h"

struct Hint : public CObject
{
    enum class Type
    {
        Added   ,
        Removed ,
        Changed ,
        All     ,
        ViewOnly
    };

    std::vector<Figure*> figures;
    Type                 type;

    Hint(Type type) : type(type)
    {}

    Hint(Type type, Figure* figure) : type(type)
    {
        figures.push_back(figure);
    }

    Hint(Type type, std::vector<Figure*> figures) : type(type), figures(figures)
    {}
};

class Model : public Observable<Hint>, public Observer<FigureAttribute>
{
    static const LONG size                   = 2000L;
    static const LONG minimumLogicalAreaSize = size / 10L;

    shos::undo_redo_pointer_vector<Figure>   figures;
    const Figure* highlightedFigure;

    FigureAttribute currentFigureAttribute;

public:
    using iterator = shos::undo_redo_vector<Figure*>::const_iterator;

    static const CSize GetMinimumSize() { return CSize(minimumLogicalAreaSize, minimumLogicalAreaSize); }
    static const CSize GetSize()        { return CSize(size, size); }
    static const CRect GetArea()        { return CRect(CPoint(), GetSize()); }

    Model() : highlightedFigure(nullptr)
    {}

    // Does not notify anyone: the observers (the document, the main window) may already be being destroyed.
    virtual ~Model()
    {
        Clear();
    }

    FigureAttribute& GetCurrentFigureAttribute()
    {
        return currentFigureAttribute;
    }

    virtual void Update(const FigureAttribute& hint) override
    {
        auto selectedFigures = GetSelectedFigures();
        if (selectedFigures.size() == 0) {
            currentFigureAttribute = hint;
            NotifyObservers(Hint(Hint::Type::ViewOnly));
        } else {
            Update(selectedFigures, hint);
        }
    }

    void Update(std::vector<Figure*> selectedFigures, const FigureAttribute& figureAttribute)
    {
        // Both the old and the new figures are notified, since the new ones can be larger (e.g. a thicker pen)
        // and the views have to redraw the union of their areas.
        std::vector<Figure*> changedFigures = selectedFigures;
        {
            shos::undo_redo_vector<Figure*>::transaction transaction(figures);
            for (auto figure : selectedFigures) {
                auto updatedFigure = figure->Clone();
                updatedFigure->Attribute() = figureAttribute;
                if (Update(*figure, *updatedFigure))
                    changedFigures.push_back(updatedFigure);
            }
        }
        InvalidateHighlight();
        NotifyObservers(Hint(Hint::Type::Changed, changedFigures));
    }

    // Replaces oldFigure with newFigure (undoable). newFigure belongs to the model from then on.
    // Returns false (and deletes newFigure) if oldFigure is not in the model.
    bool Update(Figure& oldFigure, Figure& newFigure)
    {
        auto iterator = std::find(figures.begin(), figures.end(), &oldFigure);
        if (iterator == figures.cend()) {
            delete &newFigure;
            return false;
        }
        figures.update(iterator, &newFigure);
        return true;
    }

    iterator begin() const
    {
        return figures.cbegin();
    }

    iterator end() const
    {
        return figures.cend();
    }

    void Add(Figure* figure)
    {
        ASSERT_VALID(figure);
        figure->Attribute() = currentFigureAttribute;
        figures.push_back(figure);
        InvalidateHighlight(); // The redo history may have been discarded, deleting the highlighted figure.
        NotifyObservers(Hint(Hint::Type::Added, figure));
    }

    void RemoveSelectedFigures()
    {
        auto selectedFigures = GetSelectedFigures();
        if (selectedFigures.empty()) // Nothing to do: no undo step, no notification, no modified flag.
            return;

        {
            shos::undo_redo_vector<Figure*>::transaction transaction(figures);
            std::for_each(selectedFigures.cbegin(), selectedFigures.cend(),
                [&](Figure* figure) {
                    auto iterator = std::find(figures.begin(), figures.end(), figure);
                    figures.erase(iterator);
                });
        }
        InvalidateHighlight();
        SetSelectedFigureAttribute();
        NotifyObservers(Hint(Hint::Type::Removed, selectedFigures));
    }

    bool CanRemoveSelectedFigures() const
    {
        for (auto iterator = figures.cbegin(); iterator != figures.cend(); iterator++) {
            if ((*iterator)->IsSelected())
                return true;
        }
        return false;
    }
    
    void Select(Figure& figure)
    {
        figure.Select(!figure.IsSelected());
        SetSelectedFigureAttribute();
    }

    void Select(const CRect& area)
    {
        std::for_each(figures.begin(), figures.end(),
                [&](Figure* figure) {
                         figure->Select(Geometry::InRect(area, figure->GetArea()));
                      });
        SetSelectedFigureAttribute();
    }

    void UnSelectAll()
    {
        std::for_each(figures.cbegin(), figures.cend(), [](Figure* figure) { figure->Select(false); });
        SetSelectedFigureAttribute();
    }

    void Undo()
    {
        if (figures.undo()) {
            InvalidateHighlight();
            NotifyObservers(Hint(Hint::Type::All));
        }
    }

    bool CanUndo() const
    {
        return figures.can_undo();
    }

    void Redo()
    {
        if (figures.redo()) {
            InvalidateHighlight();
            NotifyObservers(Hint(Hint::Type::All));
        }
    }

    bool CanRedo() const
    {
        return figures.can_redo();
    }

    void Hilight(const Figure* figure)
    {
        highlightedFigure = figure;
    }

    const Figure* Hilight() const
    {
        return highlightedFigure;
    }

    // Saves / loads a file. Loaded figures are not undoable: the undo history is cleared after loading.
    // (For the clipboard, use Append.)
    virtual void Serialize(CArchive& ar)
    {
        if (ar.IsStoring()) {
            ar.WriteCount(figures.size());
            for (auto figure : *this)
                ar.WriteObject(figure);
        } else {
            ReadFigures(ar);
            figures.clear_history();
            InvalidateHighlight();
            NotifyObservers(Hint(Hint::Type::All));
        }
    }

    // Appends the figures read from ar (written by Serialize) to the model. Each figure is undoable.
    void Append(CArchive& ar)
    {
        ReadFigures(ar);
        InvalidateHighlight();
    }

    // Removes all the figures and the undo history, and updates the attribute dialog and the views.
    void Reset()
    {
        Clear();
        SetSelectedFigureAttribute();
    }

    void AddDummyData(size_t count)
    {
        auto newFigures = FigureHelper::GetRandomFigures(count, GetArea());
        std::for_each(newFigures.cbegin(), newFigures.cend(), [&](Figure* figure) { figures.push_back(figure); });
        InvalidateHighlight();
        NotifyObservers(Hint(Hint::Type::Added, newFigures));
    }

private:
    // Removes all the figures and the undo history without notifying anyone.
    void Clear()
    {
        figures.reset();
        highlightedFigure = nullptr;
    }

    // The highlighted figure may have left the model (undo, removal, replacement) or even been deleted
    // (when the redo history is discarded), so it is forgotten whenever the figures change.
    // SelectCommand sets it again on the next mouse move.
    void InvalidateHighlight()
    {
        highlightedFigure = nullptr;
    }

    // Reads the figures written by Serialize and appends them to the model.
    void ReadFigures(CArchive& ar)
    {
        auto count = ar.ReadCount();
        for (DWORD_PTR counter = 0L; counter < count; counter++) {
            auto figure = STATIC_DOWNCAST(Figure, ar.ReadObject(NULL));
            if (figure != nullptr)
                figures.push_back(figure);
        }
    }

    FigureAttribute GetSelectedFigureAttribute() const
    {
        auto selectedFigures = GetSelectedFigures();

        if (selectedFigures.size() == 1)
            return selectedFigures[0]->Attribute();
        if (selectedFigures.size() > 1)
            return FigureAttribute::GetSum(GetSelectedFigureAttributes(selectedFigures));
        return currentFigureAttribute;
    }

    std::vector<FigureAttribute> GetSelectedFigureAttributes(const std::vector<Figure*> selectedFigures) const
    {
        std::vector<FigureAttribute> selectedFigureAttributes;
        for (auto figure : selectedFigures)
            selectedFigureAttributes.push_back(figure->Attribute());
        return selectedFigureAttributes;
    }
    
    void SetSelectedFigureAttribute()
    {
        Application::Set(GetSelectedFigureAttribute());
        NotifyObservers(Hint(Hint::Type::ViewOnly));
    }

    std::vector<Figure*> GetSelectedFigures() const
    {
        std::vector<Figure*> selectedFigures;
        std::for_each(figures.cbegin(), figures.cend(),
            [&](Figure* figure) {
                if (figure->IsSelected())
                    selectedFigures.push_back(figure);
            });

        return selectedFigures;
    }
};
