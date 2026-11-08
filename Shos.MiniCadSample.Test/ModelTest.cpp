// Tests of Model (MFC is used: the figures are CObjects serialized with CArchive).

#include "pch.h"
#include "CppUnitTest.h"
#include <functional>
#include <vector>
#include "AllocationFailure.h"
#include "../Shos.MiniCadSample/Model.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A serializable class that is not a Figure, to make broken clipboard data.
class NotFigure : public CObject
{
public:
    void Serialize(CArchive& ar) override
    {
        CObject::Serialize(ar);
    }

    DECLARE_SERIAL(NotFigure)
};

IMPLEMENT_SERIAL(NotFigure, CObject, 1)

namespace ShosMiniCadSampleTest
{
    // Detects memory blocks allocated in a scope and not freed (CRT debug heap; Debug builds only).
    class memory_leak_check
    {
#ifdef _DEBUG
        _CrtMemState before{};

    public:
        memory_leak_check()
        {
            _CrtMemCheckpoint(&before);
        }

        bool has_leaks() const
        {
            _CrtMemState after{}, difference{};
            _CrtMemCheckpoint(&after);
            return _CrtMemDifference(&difference, &before, &after) != FALSE;
        }
#else // _DEBUG
    public:
        bool has_leaks() const
        {
            return false;
        }
#endif // _DEBUG
    };

    // Writes with write into a memory file and returns its bytes.
    std::vector<BYTE> write_data(const std::function<void(CArchive&)>& write)
    {
        CMemFile file;
        {
            CArchive ar(&file, CArchive::store);
            write(ar);
            ar.Close();
        }
        const auto length = static_cast<size_t>(file.GetLength());
        std::vector<BYTE> bytes(length);
        file.SeekToBegin();
        file.Read(bytes.data(), static_cast<UINT>(length));
        return bytes;
    }

    // Pastes bytes into model. Exceptions propagate.
    void paste(Model& model, std::vector<BYTE> bytes)
    {
        CMemFile file(bytes.data(), static_cast<UINT>(bytes.size()));
        CArchive ar(&file, CArchive::load);
        try {
            model.Paste(ar);
            ar.Close();
        } catch (...) {
            ar.Abort();
            throw;
        }
    }

    // Pastes bytes into model, expecting a CException. Returns whether one was thrown.
    bool paste_fails(Model& model, const std::vector<BYTE>& bytes)
    {
        try {
            paste(model, bytes);
        } catch (CException* exception) {
            exception->Delete();
            return true;
        }
        return false;
    }

    std::vector<CRect> areas_of(const Model& model)
    {
        std::vector<CRect> areas;
        for (auto figure : model)
            areas.push_back(figure->GetArea());
        return areas;
    }

    std::vector<bool> selection_of(const Model& model)
    {
        std::vector<bool> selection;
        for (auto figure : model)
            selection.push_back(figure->IsSelected());
        return selection;
    }

    // The observable state of a model, to check that a failed operation changed nothing.
    struct model_state
    {
        std::vector<CRect> areas;
        std::vector<bool>  selection;
        bool               canUndo;
        bool               canRedo;

        explicit model_state(const Model& model)
            : areas(areas_of(model)), selection(selection_of(model)), canUndo(model.CanUndo()), canRedo(model.CanRedo())
        {}

        void assert_equal(const Model& model) const
        {
            Assert::IsTrue(areas     == areas_of(model)    , L"figures changed");
            Assert::IsTrue(selection == selection_of(model), L"selection changed");
            Assert::AreEqual(canUndo, model.CanUndo(), L"can undo changed");
            Assert::AreEqual(canRedo, model.CanRedo(), L"can redo changed");
        }
    };

    TEST_CLASS(model_clipboard_test)
    {
        // A model with figures far from the origin and an undo / redo history.
        static void prepare(Model& model)
        {
            model.Add(new RectangleFigure(CRect(1000, 1200, 1100, 1300)));
            model.Add(new EllipseFigure  (CRect(1500, 1500, 1600, 1550)));
            model.Add(new LineFigure     (CPoint(800, 900), CPoint(850, 950)));
            model.Add(new DotFigure      (CPoint(10, 10)));
            model.Undo(); // The dot is held by the redo history.
        }

        static Figure* figure_at(const Model& model, size_t index)
        {
            return *std::next(model.begin(), index);
        }

    public:
        TEST_METHOD(store_and_paste_selected_figures)
        {
            Model source;
            prepare(source);
            source.Select(*figure_at(source, 0));
            source.Select(*figure_at(source, 1));

            // The output area is the bounding rectangle of the selected figures, not of the document.
            const auto area = source.GetFiguresArea(true);
            CRect expected;
            expected.UnionRect(figure_at(source, 0)->GetArea(), figure_at(source, 1)->GetArea());
            Assert::IsTrue(area == expected);
            Assert::IsTrue(area.left > 0 && area.top > 0);

            const auto bytes = write_data([&](CArchive& ar) { source.StoreFigures(ar, true); });

            Model target;
            prepare(target);
            target.Select(*figure_at(target, 2));
            paste(target, bytes);

            // Appended at the end and selected; the others are unselected.
            Assert::AreEqual<size_t>(5, areas_of(target).size());
            Assert::IsTrue(figure_at(target, 3)->GetArea() == figure_at(source, 0)->GetArea());
            Assert::IsTrue(figure_at(target, 4)->GetArea() == figure_at(source, 1)->GetArea());
            Assert::IsTrue(std::vector<bool>{ false, false, false, true, true } == selection_of(target));
            Assert::IsFalse(target.CanRedo()); // The redo history was discarded.

            // One undo removes the whole paste.
            target.Undo();
            Assert::AreEqual<size_t>(3, areas_of(target).size());
            target.Redo();
            Assert::AreEqual<size_t>(5, areas_of(target).size());
        }

        TEST_METHOD(store_all_figures_without_selection)
        {
            Model source;
            prepare(source);
            Assert::IsFalse(source.HasSelection());

            const auto bytes = write_data([&](CArchive& ar) { source.StoreFigures(ar, false); });

            Model target;
            paste(target, bytes);
            Assert::IsTrue(areas_of(source) == areas_of(target));
        }

        TEST_METHOD(paste_empty_data)
        {
            Model source;
            const auto bytes = write_data([&](CArchive& ar) { source.StoreFigures(ar, false); });

            Model target;
            prepare(target);
            const model_state state(target);
            paste(target, bytes);
            state.assert_equal(target);
        }

        TEST_METHOD(paste_truncated_data)
        {
            // The count says 3, but only 2 figures follow.
            const auto bytes = write_data([](CArchive& ar) {
                RectangleFigure first (CRect(0, 0, 10, 10));
                EllipseFigure   second(CRect(0, 0, 20, 20));
                ar.WriteCount(3);
                ar.WriteObject(&first);
                ar.WriteObject(&second);
            });
            assert_paste_fails_without_change(bytes);
        }

        TEST_METHOD(paste_object_that_is_not_a_figure)
        {
            const auto bytes = write_data([](CArchive& ar) {
                RectangleFigure figure(CRect(0, 0, 10, 10));
                NotFigure       notFigure;
                ar.WriteCount(2);
                ar.WriteObject(&figure);
                ar.WriteObject(&notFigure);
            });
            assert_paste_fails_without_change(bytes);
        }

        TEST_METHOD(paste_duplicated_reference)
        {
            // CArchive writes the second one as a reference to the first; reading gives the same object twice,
            // which must not be owned (and deleted) twice.
            const auto bytes = write_data([](CArchive& ar) {
                RectangleFigure figure(CRect(0, 0, 10, 10));
                ar.WriteCount(2);
                ar.WriteObject(&figure);
                ar.WriteObject(&figure);
            });
            assert_paste_fails_without_change(bytes);
        }

        TEST_METHOD(paste_allocation_failure)
        {
            Model source;
            prepare(source);
            const auto bytes = write_data([&](CArchive& ar) { source.StoreFigures(ar, false); });

            Model target;
            prepare(target);
            paste(target, bytes); // Warms up the lazy allocations of MFC, so that they are not taken for leaks.
            target.Undo();

            int failures = 0;
            for (int succeeding = 0; ; succeeding++) {
                const model_state state(target);
                bool failed = false;
                {
                    const memory_leak_check check;
                    try {
                        allocation_failure failure(succeeding);
                        paste(target, bytes);
                    } catch (const std::bad_alloc&) {
                        failed = true;
                    }
                    if (failed)
                        Assert::IsFalse(check.has_leaks(), L"leaked after a failed paste");
                }
                if (!failed)
                    break;
                failures++;
                state.assert_equal(target);
            }
            Assert::IsTrue(failures > 0);
            Assert::AreEqual<size_t>(6, areas_of(target).size());
            target.Undo();
            Assert::AreEqual<size_t>(3, areas_of(target).size());
        }

    private:
        static void assert_paste_fails_without_change(const std::vector<BYTE>& bytes)
        {
            Model target;
            prepare(target);
            target.Select(*figure_at(target, 1));
            {
                // Warms up the lazy allocations of MFC (CArchive, CRuntimeClass lookup).
                Model warmUp;
                (void)paste_fails(warmUp, bytes);
            }
            const model_state state(target);
            {
                const memory_leak_check check;
                Assert::IsTrue(paste_fails(target, bytes), L"no exception");
                Assert::IsFalse(check.has_leaks(), L"leaked after a failed paste");
            }
            state.assert_equal(target);
        }
    };

    TEST_CLASS(model_test)
    {
    public:
        TEST_METHOD(remove_selected_figures_without_selection_does_nothing)
        {
            Model model;
            model.Add(new DotFigure(CPoint(10, 10)));

            int notifications = 0;
            struct counter : Observer<Hint>
            {
                int& count;
                explicit counter(int& count) : count(count) {}
                void Update(const Hint&) override { count++; }
            } observer(notifications);
            model.AddObserver(observer);

            model.RemoveSelectedFigures();
            Assert::AreEqual(0, notifications);
            Assert::AreEqual<size_t>(1, areas_of(model).size());
        }

        TEST_METHOD(dummy_data_is_one_undo_step)
        {
            Model model;
            model.Add(new DotFigure(CPoint(10, 10)));
            model.AddDummyData(10);
            Assert::AreEqual<size_t>(11, areas_of(model).size());

            for (auto figure : model) {
                const auto penWidth = figure->Attribute().GetPenWidth();
                Assert::IsTrue(0 <= penWidth && penWidth <= 5);
            }

            model.Undo();
            Assert::AreEqual<size_t>(1, areas_of(model).size());
            model.Redo();
            Assert::AreEqual<size_t>(11, areas_of(model).size());
        }

        // H3: changing the color of figures with different pen widths keeps each pen width.
        TEST_METHOD(attribute_change_applies_only_changed_item)
        {
            Model model;
            auto first  = new RectangleFigure(CRect(0, 0, 10, 10));
            auto second = new EllipseFigure  (CRect(0, 0, 20, 20));
            // Add sets the attribute for new figures.
            model.GetCurrentFigureAttribute().SetPenWidth(1);
            model.Add(first);
            model.GetCurrentFigureAttribute().SetPenWidth(3);
            model.Add(second);
            model.Select(*first);
            model.Select(*second);

            auto change = FigureAttribute::Empty();
            change.SetColor(RGB(9, 9, 9));
            model.Update(change);

            auto updatedFirst  = *model.begin();
            auto updatedSecond = *std::next(model.begin());
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), updatedFirst ->Attribute().GetColor());
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), updatedSecond->Attribute().GetColor());
            Assert::AreEqual(1, updatedFirst ->Attribute().GetPenWidth());
            Assert::AreEqual(3, updatedSecond->Attribute().GetPenWidth());
            Assert::IsTrue(updatedFirst ->Attribute().IsPenWidthValid());
            Assert::IsTrue(updatedSecond->Attribute().IsPenWidthValid());

            // One undo restores both.
            model.Undo();
            Assert::AreEqual<COLORREF>(RGB(0, 0, 0), (*model.begin())->Attribute().GetColor());
            Assert::AreEqual<COLORREF>(RGB(0, 0, 0), (*std::next(model.begin()))->Attribute().GetColor());
            Assert::IsTrue(model.CanUndo()); // The two additions remain.
            model.Undo();
            model.Undo();
            Assert::IsFalse(model.CanUndo());
        }

        TEST_METHOD(attribute_change_without_effect_adds_no_undo_step)
        {
            Model model;
            auto figure = new DotFigure(CPoint(10, 10));
            model.GetCurrentFigureAttribute().SetPenWidth(2); // Add sets the attribute for new figures.
            model.Add(figure);
            model.Select(*figure);

            int notifications = 0;
            struct counter : Observer<Hint>
            {
                int& count;
                explicit counter(int& count) : count(count) {}
                void Update(const Hint&) override { count++; }
            } observer(notifications);
            model.AddObserver(observer);

            auto change = FigureAttribute::Empty();
            change.SetPenWidth(2);
            model.Update(change);
            Assert::AreEqual(0, notifications);
            Assert::IsTrue(*model.begin() == figure); // Not replaced.
            model.Undo();
            Assert::IsFalse(model.CanUndo()); // The only step was the addition.
        }

        TEST_METHOD(attribute_change_without_selection_changes_attribute_for_new_figures)
        {
            Model model;
            model.GetCurrentFigureAttribute().SetPenWidth(5);

            auto change = FigureAttribute::Empty();
            change.SetColor(RGB(9, 9, 9));
            model.Update(change);

            const auto& current = model.GetCurrentFigureAttribute();
            Assert::AreEqual<COLORREF>(RGB(9, 9, 9), current.GetColor());
            Assert::AreEqual(5, current.GetPenWidth());
            Assert::IsTrue(current.IsColorValid());
            Assert::IsTrue(current.IsPenWidthValid());
            Assert::IsFalse(model.CanUndo());
        }
    };
}
