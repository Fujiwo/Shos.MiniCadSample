#pragma once

#include <afx.h>
#include <memory>
#include <vector>
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

    // Applies a change from the attribute dialog: only its valid items (see FigureAttribute::ApplyTo).
    // With a selection, the selected figures are changed (one undo step); otherwise, the attribute for new figures.
    virtual void Update(const FigureAttribute& change) override
    {
        auto selectedFigures = GetSelectedFigures();
        if (selectedFigures.size() == 0) {
            change.ApplyTo(currentFigureAttribute);
            NotifyObservers(Hint(Hint::Type::ViewOnly));
        } else {
            Update(selectedFigures, change);
        }
    }

    void Update(const std::vector<Figure*>& targetFigures, const FigureAttribute& change)
    {
        // Both the old and the new figures are notified, since the new ones can be larger (e.g. a thicker pen)
        // and the views have to redraw the union of their areas.
        std::vector<Figure*> changedFigures;
        changedFigures.reserve(targetFigures.size() * 2);
        {
            shos::undo_redo_vector<Figure*>::transaction transaction(figures);
            for (auto figure : targetFigures) {
                std::unique_ptr<Figure> updatedFigure(figure->Clone());
                if (!change.ApplyTo(updatedFigure->Attribute()))
                    continue; // Already has the attribute: no undo step.
                auto newFigure = updatedFigure.release();
                if (Update(*figure, *newFigure)) {
                    changedFigures.push_back(figure);
                    changedFigures.push_back(newFigure);
                }
            }
        }
        if (changedFigures.empty())
            return;
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
        return HasSelection();
    }

    bool HasSelection() const
    {
        return std::any_of(figures.cbegin(), figures.cend(), [](const Figure* figure) { return figure->IsSelected(); });
    }

    bool IsEmpty() const
    {
        return figures.empty();
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

    void Highlight(const Figure* figure)
    {
        highlightedFigure = figure;
    }

    const Figure* Highlight() const
    {
        return highlightedFigure;
    }

    // Saves / loads a file. Loaded figures are not undoable: the undo history is cleared after loading.
    // Loading is all or nothing: if the file is broken, the exception propagates and no figure is added.
    // (For the clipboard, use StoreFigures / Paste.)
    virtual void Serialize(CArchive& ar)
    {
        if (ar.IsStoring()) {
            StoreFigures(ar, false);
        } else {
            auto loaded = ReadFigures(ar);
            CommitFigures(loaded);
            figures.clear_history();
            InvalidateHighlight();
            NotifyObservers(Hint(Hint::Type::All));
        }
    }

    // ---- Clipboard ----

    // Writes the figures (only the selected ones if selectedOnly) in the format read by Serialize / Paste.
    void StoreFigures(CArchive& ar, bool selectedOnly) const
    {
        const auto targets = GetFigures(selectedOnly);
        ar.WriteCount(targets.size());
        for (auto figure : targets)
            ar.WriteObject(figure);
    }

    // Returns the area that the figures (only the selected ones if selectedOnly) occupy, including their
    // pen width. Returns an empty rectangle if there is none.
    CRect GetFiguresArea(bool selectedOnly) const
    {
        CRect area;
        FigureHelper::GetArea(GetFigures(selectedOnly), area);
        return area;
    }

    // Draws the figures (only the selected ones if selectedOnly) without the selection markers.
    void DrawFigures(CDC& dc, bool selectedOnly) const
    {
        for (auto figure : GetFigures(selectedOnly))
            figure->Draw(dc, false);
    }

    // Appends the figures read from ar (written by StoreFigures) as one undo step, and selects them (only them).
    // All or nothing: if reading or adding fails, the exception (CArchiveException* etc. or std::bad_alloc)
    // propagates and the model is unchanged.
    void Paste(CArchive& ar)
    {
        auto loaded = ReadFigures(ar);
        if (loaded.empty())
            return;

        // Everything that allocates is prepared before the model is changed.
        std::vector<Figure*> pasted;
        pasted.reserve(loaded.size());
        for (const auto& figure : loaded)
            pasted.push_back(figure.get());
        const Hint addedHint(Hint::Type::Added, pasted);
        const Hint viewOnlyHint(Hint::Type::ViewOnly);
        const auto attribute = GetAttribute(pasted); // The attribute of the selection after pasting.

        CommitFigures(loaded); // All or nothing.

        // The model does not throw from here on.
        std::for_each(figures.cbegin(), figures.cend(), [](Figure* figure) { figure->Select(false); });
        std::for_each(pasted.cbegin(), pasted.cend(), [](Figure* figure) { figure->Select(true); });
        InvalidateHighlight();
        NotifyObservers(addedHint);
        Application::Set(attribute);
        NotifyObservers(viewOnlyHint);
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

    std::vector<Figure*> GetFigures(bool selectedOnly) const
    {
        return selectedOnly ? GetSelectedFigures() : std::vector<Figure*>(figures.cbegin(), figures.cend());
    }

    // Reads the figures written by StoreFigures into a temporary list, without changing the model.
    // Throws CArchiveException* if the data is broken; the figures read so far are deleted.
    static std::vector<std::unique_ptr<Figure>> ReadFigures(CArchive& ar)
    {
        std::vector<std::unique_ptr<Figure>> loaded;
        const auto count = ar.ReadCount(); // Not trusted for reserve: broken data could claim any number.
        for (DWORD_PTR counter = 0; counter < count; counter++) {
            std::unique_ptr<CObject> object(ar.ReadObject(nullptr));
            if (object == nullptr) // A null pointer was written.
                continue;

            auto figure = DYNAMIC_DOWNCAST(Figure, object.get());
            if (figure == nullptr)
                AfxThrowArchiveException(CArchiveException::badClass);
            // CArchive returns the same object again for a back reference; owning it twice would delete it twice.
            if (std::any_of(loaded.cbegin(), loaded.cend(), [&](const std::unique_ptr<Figure>& each) { return each.get() == figure; })) {
                object.release();
                AfxThrowArchiveException(CArchiveException::badIndex);
            }

            loaded.emplace_back(figure);
            object.release(); // Now owned by loaded.
        }
        return loaded;
    }

    // Appends the loaded figures to the model as one undo step, taking over their ownership (loaded is emptied).
    // All or nothing: on failure the exception propagates, the model is unchanged and the figures are deleted.
    // Does not notify.
    void CommitFigures(std::vector<std::unique_ptr<Figure>>& loaded)
    {
        std::vector<Figure*> elements;
        elements.reserve(loaded.size()); // May throw; loaded still owns the figures.
        for (const auto& figure : loaded)
            elements.push_back(figure.get());

        // Hand the ownership over to figures.append, which deletes them all if it fails.
        for (auto& figure : loaded)
            figure.release();
        loaded.clear();
        figures.append(std::move(elements));
    }

    FigureAttribute GetSelectedFigureAttribute() const
    {
        return GetAttribute(GetSelectedFigures());
    }

    // The attribute shown in the dialog when targetFigures are selected:
    // theirs (merged if more than one), or the one for new figures if there is none.
    FigureAttribute GetAttribute(const std::vector<Figure*>& targetFigures) const
    {
        if (targetFigures.size() == 1)
            return targetFigures[0]->Attribute();
        if (targetFigures.size() > 1)
            return FigureAttribute::GetSum(GetSelectedFigureAttributes(targetFigures));
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
