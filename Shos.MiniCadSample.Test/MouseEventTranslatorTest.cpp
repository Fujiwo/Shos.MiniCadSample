// Tests of MouseEventTranslator: how raw mouse messages become click / cursor / drag events,
// in particular when the other button is pressed or the button is released outside the view.

#include "pch.h"
#include "CppUnitTest.h"
#include <memory>
#include <string>
#include <vector>
#include "../Shos.MiniCadSample/MouseEventTranslator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Shos::MiniCadSample::Controllers;

namespace ShosMiniCadSampleTest
{
    // A view attached to a hidden window. MouseEventTranslator converts points with a CClientDC, which needs a real window.
    // The window is a plain Win32 STATIC window created without MFC: the test DLL has no CWinApp, so letting MFC
    // create a CView window (PreCreateWindow, AfxRegisterWndClass) asserts or fails.
    // The default mapping (MM_TEXT, no scrolling) makes logical points equal to device points.
    class TestView : public CView
    {
    public:
        bool Create()
        {
            auto window = ::CreateWindowEx(0, _T("STATIC"), _T("MouseEventTranslatorTest"), WS_POPUP,
                                           0, 0, 100, 100, nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
            return window != nullptr && Attach(window) != FALSE;
        }

        void Destroy()
        {
            auto window = Detach();
            if (window != nullptr)
                ::DestroyWindow(window);
        }

    protected:
        virtual void OnDraw(CDC* /* pDC */) override
        {}
    };

    // Records every event as text, e.g. "DragStart L 1,2".
    class RecordingListener : public MouseEventTranslator::Listener
    {
    public:
        std::vector<std::wstring> events;

        virtual void OnClick        (CPoint point)            override { Record(L"Click"    , point); }
        virtual void OnCursor       (CPoint point)            override { Record(L"Cursor"   , point); }
        virtual void OnDragStart    (UINT keys, CPoint point) override { Record(L"DragStart", keys, point); }
        virtual void OnDragging     (UINT keys, CPoint point) override { Record(L"Dragging" , keys, point); }
        virtual void OnDraggingAbort()                        override { events.push_back(L"Abort"); }
        virtual void OnDragEnd      (UINT keys, CPoint point) override { Record(L"DragEnd"  , keys, point); }

    private:
        void Record(const std::wstring& name, CPoint point)
        {
            events.push_back(name + L" " + ToString(point));
        }

        void Record(const std::wstring& name, UINT keys, CPoint point)
        {
            std::wstring keysText;
            if ((keys & MK_LBUTTON) != 0U)
                keysText += L"L";
            if ((keys & MK_RBUTTON) != 0U)
                keysText += L"R";
            events.push_back(name + L" " + keysText + L" " + ToString(point));
        }

        static std::wstring ToString(CPoint point)
        {
            return std::to_wstring(point.x) + L"," + std::to_wstring(point.y);
        }
    };

    TEST_CLASS(MouseEventTranslatorTest)
    {
        std::unique_ptr<TestView>             view;
        std::unique_ptr<MouseEventTranslator> translator;
        RecordingListener                     listener;

        // Points far enough apart to start a drag (dragStartingDistance is 5).
        inline static const CPoint p0 = CPoint( 0, 0);
        inline static const CPoint p1 = CPoint(20, 0);
        inline static const CPoint p2 = CPoint(40, 0);

        void AssertEvents(const std::vector<std::wstring>& expected)
        {
            Assert::AreEqual(expected.size(), listener.events.size());
            for (size_t index = 0; index < expected.size(); ++index)
                Assert::AreEqual(expected[index], listener.events[index]);
        }

    public:
        TEST_METHOD_INITIALIZE(Initialize)
        {
            view = std::make_unique<TestView>();
            Assert::IsTrue(view->Create());
            translator = std::make_unique<MouseEventTranslator>(*view);
            translator->AddListener(listener);
        }

        TEST_METHOD_CLEANUP(Cleanup)
        {
            translator.reset();
            view->Destroy();
            view.reset();
        }

        TEST_METHOD(left_click)
        {
            translator->OnLButtonDown(MK_LBUTTON, p0);
            translator->OnLButtonUp  (0U        , p0);
            AssertEvents({ L"Click 0,0" });
        }

        // The right button only drags: a right click produces no event.
        TEST_METHOD(right_click_is_ignored)
        {
            translator->OnRButtonDown(MK_RBUTTON, p0);
            translator->OnRButtonUp  (0U        , p0);
            AssertEvents({});
        }

        TEST_METHOD(left_drag)
        {
            translator->OnLButtonDown(MK_LBUTTON, p0);
            translator->OnMouseMove  (MK_LBUTTON, p1);
            translator->OnLButtonUp  (0U        , p2);
            AssertEvents({ L"DragStart L 0,0", L"Dragging L 20,0", L"DragEnd L 40,0" });
        }

        // Pressing and releasing the right button during a left drag neither breaks the drag nor adds events,
        // and the moves while both buttons are down are reported with the left button only.
        TEST_METHOD(right_button_during_left_drag_is_ignored)
        {
            translator->OnLButtonDown(MK_LBUTTON             , p0);
            translator->OnMouseMove  (MK_LBUTTON             , p1);
            translator->OnRButtonDown(MK_LBUTTON | MK_RBUTTON, p1);
            translator->OnMouseMove  (MK_LBUTTON | MK_RBUTTON, p2);
            translator->OnRButtonUp  (MK_LBUTTON             , p2);
            translator->OnLButtonUp  (0U                     , p2);
            AssertEvents({ L"DragStart L 0,0", L"Dragging L 20,0", L"Dragging L 40,0", L"DragEnd L 40,0" });
        }

        TEST_METHOD(left_button_during_right_drag_is_ignored)
        {
            translator->OnRButtonDown(MK_RBUTTON             , p0);
            translator->OnMouseMove  (MK_RBUTTON             , p1);
            translator->OnLButtonDown(MK_LBUTTON | MK_RBUTTON, p1);
            translator->OnLButtonUp  (MK_RBUTTON             , p1);
            translator->OnRButtonUp  (0U                     , p2);
            AssertEvents({ L"DragStart R 0,0", L"Dragging R 20,0", L"DragEnd R 40,0" });
        }

        // A left click stays a click even if the right button is pressed and released before the left is released.
        TEST_METHOD(right_button_during_left_click_is_ignored)
        {
            translator->OnLButtonDown(MK_LBUTTON             , p0);
            translator->OnRButtonDown(MK_LBUTTON | MK_RBUTTON, p0);
            translator->OnRButtonUp  (MK_LBUTTON             , p0);
            translator->OnLButtonUp  (0U                     , p0);
            AssertEvents({ L"Click 0,0" });
        }

        // If the button was released where the view did not see it, the next move aborts the drag.
        TEST_METHOD(release_not_seen_aborts_drag)
        {
            translator->OnLButtonDown(MK_LBUTTON, p0);
            translator->OnMouseMove  (MK_LBUTTON, p1);
            translator->OnMouseMove  (0U        , p2);
            AssertEvents({ L"DragStart L 0,0", L"Dragging L 20,0", L"Abort", L"Cursor 40,0" });
        }

        // After a release that was not seen, a press of the other button is tracked normally.
        TEST_METHOD(release_not_seen_then_other_button_drags)
        {
            translator->OnLButtonDown(MK_LBUTTON, p0);
            translator->OnMouseMove  (MK_LBUTTON, p1);
            translator->OnRButtonDown(MK_RBUTTON, p1);
            translator->OnMouseMove  (MK_RBUTTON, p2);
            translator->OnRButtonUp  (0U        , p2);
            AssertEvents({ L"DragStart L 0,0", L"Dragging L 20,0", L"Abort",
                           L"DragStart R 20,0", L"Dragging R 40,0", L"DragEnd R 40,0" });
        }

        TEST_METHOD(mouse_leave_aborts_drag)
        {
            translator->OnLButtonDown(MK_LBUTTON, p0);
            translator->OnMouseMove  (MK_LBUTTON, p1);
            translator->OnMouseLeave();
            translator->OnLButtonUp  (0U        , p2);
            AssertEvents({ L"DragStart L 0,0", L"Dragging L 20,0", L"Abort" });
        }
    };
}
