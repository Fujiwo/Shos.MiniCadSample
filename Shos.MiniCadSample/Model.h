#pragma once

// Model: owns the figures and the figure attribute for new figures. Every change is undoable (except
// selection) and is announced to the observers (document, views) as a Hint.

#include <afx.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>
#include "Observer.h"
#include "Figure.h"
#include "Application.h"
#include "undo_redo_vector.h"

namespace Shos::MiniCadSample::Models {

using namespace Utilities;

// What a notification from Model tells the views, so that they can redraw only what changed.
struct Hint : public CObject
{
    enum class Type
    {
        Added   , // figures were added.
        Removed , // figures were removed (they are still alive in the undo history).
        Changed , // figures were replaced: contains both the old and the new ones.
        All     , // Anything may have changed (undo, redo, load): redraw everything.
        ViewOnly  // Only view state changed (selection, attribute): no figure was added or removed.
    };

    std::vector<Figure*> figures; // The affected figures (not owned). Empty for All and ViewOnly.
    Type                 type;

    explicit Hint(Type type) : type(type)
    {}

    Hint(Type type, Figure* figure) : figures{ figure }, type(type)
    {}

    Hint(Type type, std::vector<Figure*> figures) : figures(std::move(figures)), type(type)
    {}
};

// The document data. Figures live in the logical area (0, 0)-(size, size). The model owns the figures
// (including those in the undo history), so a figure given to it must never be deleted by the caller.
// Observers: Observable<Hint> for changes, and Observer<FigureAttribute> to receive edits from the dialog.
class Model : public Observable<Hint>, public Observer<FigureAttribute>
{
    static constexpr LONG size                   = 2000L;       // Side of the square logical area.
    static constexpr LONG minimumLogicalAreaSize = size / 10L;  // The smallest area a view may show (the zoom limit).

    shos::undo_redo_pointer_vector<Figure> figures;                   // The figures, in drawing order, with the undo history.
    const Figure*                          highlightedFigure = nullptr; // The figure under the cursor (not owned), or nullptr.

    FigureAttribute currentFigureAttribute; // The attribute given to new figures.

public:
    // Iterates over the figures (read-only pointers to the slots; the figures themselves can be Select()ed).
    using iterator = shos::undo_redo_vector<Figure*>::const_iterator;

    // The area that figures can be drawn in, and the smallest part of it a view may show.
    static CSize GetMinimumSize() { return CSize(minimumLogicalAreaSize, minimumLogicalAreaSize); }
    static CSize GetSize()        { return CSize(size, size); }
    static CRect GetArea()        { return CRect(CPoint(), GetSize()); }

    Model() = default;

    // Does not notify anyone: the observers (the document, the main window) may already be being destroyed.
    virtual ~Model()
    {
        Clear();
    }

    // The attribute given to new figures (shown in the dialog when nothing is selected).
    FigureAttribute& GetCurrentFigureAttribute()
    {
        return currentFigureAttribute;
    }

    // Applies a change from the attribute dialog: only its valid items (see FigureAttribute::ApplyTo).
    // With a selection, the selected figures are changed (one undo step); otherwise, the attribute for new figures.
    virtual void Update(const FigureAttribute& change) override
    {
        auto selectedFigures = GetSelectedFigures();
        if (selectedFigures.empty()) {
            change.ApplyTo(currentFigureAttribute);
            NotifyObservers(Hint(Hint::Type::ViewOnly));
        } else {
            Update(selectedFigures, change);
        }
    }

    // Applies change to targetFigures as one undo step. Each figure is cloned and replaced, so the old
    // ones stay in the undo history. Figures that the change does not affect are left alone.
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
        const auto iterator = std::ranges::find(figures, &oldFigure);
        if (iterator == figures.cend()) {
            delete &newFigure;
            return false;
        }
        figures.update(iterator, &newFigure);
        return true;
    }

    // Range access to the figures (so that range-for and std::ranges work on a Model).
    iterator begin() const
    {
        return figures.cbegin();
    }

    iterator end() const
    {
        return figures.cend();
    }

    // Adds figure (the model owns it from then on) with the attribute for new figures. Undoable.
    void Add(Figure* figure)
    {
        ASSERT_VALID(figure);
        figure->Attribute() = currentFigureAttribute;
        figures.push_back(figure);
        InvalidateHighlight(); // The redo history may have been discarded, deleting the highlighted figure.
        NotifyObservers(Hint(Hint::Type::Added, figure));
    }

    // Removes the selected figures as one undo step.
    void RemoveSelectedFigures()
    {
        auto selectedFigures = GetSelectedFigures();
        if (selectedFigures.empty()) // Nothing to do: no undo step, no notification, no modified flag.
            return;

        {
            shos::undo_redo_vector<Figure*>::transaction transaction(figures);
            for (auto figure : selectedFigures)
                figures.erase(std::ranges::find(figures, figure));
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
        return std::ranges::any_of(figures, &Figure::IsSelected);
    }

    bool IsEmpty() const
    {
        return figures.empty();
    }
    
    // Toggles the selection of figure and updates the attribute dialog. Not undoable.
    void Select(Figure& figure)
    {
        figure.Select(!figure.IsSelected());
        SetSelectedFigureAttribute();
    }

    // Selects the figures whose area is inside area (rubber-band selection) and unselects the others.
    void Select(const CRect& area)
    {
        for (auto figure : figures)
            figure->Select(Geometry::InRect(area, figure->GetArea()));
        SetSelectedFigureAttribute();
    }

    // Unselects every figure and updates the attribute dialog.
    void UnSelectAll()
    {
        for (auto figure : figures)
            figure->Select(false);
        SetSelectedFigureAttribute();
    }

    // Undoes / redoes the last step (if any) and notifies the views to redraw everything.
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

    // The figure under the cursor, drawn emphasized by the view. Not a model change: no notification, no undo.
    // The pointer is reset to nullptr whenever the figures change (see InvalidateHighlight).
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
        for (auto figure : figures)
            figure->Select(false);
        for (auto figure : pasted)
            figure->Select(true);
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

    // Adds count random figures as one undo step.
    void AddDummyData(size_t count)
    {
        auto newFigures = FigureHelper::GetRandomFigures(count, GetArea());
        std::unique_ptr<Hint> hint;
        try {
            hint = std::make_unique<Hint>(Hint::Type::Added, newFigures); // Prepared before the commit, which takes newFigures.
        } catch (...) {
            for (auto figure : newFigures)
                delete figure;
            throw;
        }
        figures.append(std::move(newFigures)); // All or nothing; on failure, the figures are deleted.
        InvalidateHighlight();
        NotifyObservers(*hint);
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

    // Pointers to all the figures, or to the selected ones only.
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
            if (std::ranges::any_of(loaded, [&](const auto& each) { return each.get() == figure; })) {
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

    // The attribute for the current selection (see GetAttribute).
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

    // Copies the attributes of the figures (the input of FigureAttribute::GetSum).
    static std::vector<FigureAttribute> GetSelectedFigureAttributes(const std::vector<Figure*>& selectedFigures)
    {
        std::vector<FigureAttribute> selectedFigureAttributes;
        selectedFigureAttributes.reserve(selectedFigures.size());
        for (const Figure* figure : selectedFigures)
            selectedFigureAttributes.push_back(figure->Attribute());
        return selectedFigureAttributes;
    }
    
    // Pushes the attribute of the current selection to the dialog (Application::Set) and tells the views
    // to redraw (selection markers). Called whenever the selection or the figures change.
    void SetSelectedFigureAttribute()
    {
        Application::Set(GetSelectedFigureAttribute());
        NotifyObservers(Hint(Hint::Type::ViewOnly));
    }

    // Pointers to the selected figures, in drawing order.
    std::vector<Figure*> GetSelectedFigures() const
    {
        std::vector<Figure*> selectedFigures;
        std::ranges::copy_if(figures, std::back_inserter(selectedFigures), &Figure::IsSelected);
        return selectedFigures;
    }
};

} // namespace Shos::MiniCadSample::Models
