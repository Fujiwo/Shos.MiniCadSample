#pragma once
// Translates raw mouse messages from a view into higher-level events (click, cursor move, drag) for listeners.

#ifdef _DEBUG
// Define this (Debug only) to enable TestListener, which traces every event.
//#define MOUSE_EVENT_TRANSLATOR_TEST
#endif // _DEBUG

#include <afx.h>
#include <algorithm>
#include <concepts>
#include <ranges>
#include <vector>
#include "Geometry.h"

namespace Shos::MiniCadSample::Controllers {

using namespace Utilities;

// Turns raw mouse input (button down/up, move, leave; device coordinates, as the view receives them) into
// click, cursor, drag start, dragging, drag abort and drag end events. Listeners receive logical coordinates.
// A press followed by a release without moving farther than dragStartingDistance is a click; a larger move
// starts a drag. The left and right buttons are both tracked, and the keys passed to the drag events say which.
// Only one button is tracked at a time: while one is held, the other one is ignored, so pressing it does not break
// the click or drag in progress.
class MouseEventTranslator
{
public:
    // Receiver of the translated events. All points are logical coordinates. Every handler does nothing by default.
    class Listener
    {
    public:
        virtual ~Listener() = 0;

        // The left button was pressed and released without a drag.
        virtual void OnClick        (CPoint /* point */) {}
        // The mouse moved with no button pressed.
        virtual void OnCursor       (CPoint /* point */) {}
        // A drag began. point is where the button was pressed (not where the drag was detected). keys has MK_LBUTTON or MK_RBUTTON.
        virtual void OnDragStart    (UINT /* keys */, CPoint /* point */) {}
        // The mouse moved during a drag. The points seen before the drag was detected are replayed after OnDragStart.
        virtual void OnDragging     (UINT /* keys */, CPoint /* point */) {}
        // The drag was cancelled (the mouse left the view, or the button was released where the view did not see it).
        // No OnDragEnd follows.
        virtual void OnDraggingAbort() {}
        // The button was released at the end of a drag.
        virtual void OnDragEnd      (UINT /* keys */, CPoint /* point */) {}
    };

#ifdef MOUSE_EVENT_TRANSLATOR_TEST
    // Debug listener that writes each event to the debugger output.
    class TestListener : public MouseEventTranslator::Listener
    {
    public:
        virtual void OnClick        (CPoint point)            override { Trace(_T("OnClick"        ),       point); }
        virtual void OnCursor       (CPoint point)            override { Trace(_T("OnCursor"       ),       point); }
        virtual void OnDragStart    (UINT keys, CPoint point) override { Trace(_T("OnDragStart"    ), keys, point); }
        virtual void OnDragging     (UINT keys, CPoint point) override { Trace(_T("OnDragging"     ), keys, point); }
        virtual void OnDraggingAbort()                        override { Trace(_T("OnDraggingAbort")             ); }
        virtual void OnDragEnd      (UINT keys, CPoint point) override { Trace(_T("OnDragEnd"      ), keys, point); }

    private:
        void Trace(LPCTSTR methodName)
        {
            TraceMethodName(methodName);
        }

        void Trace(LPCTSTR methodName, CPoint point)
        {
            TraceMethodName(methodName);
            TRACE(_T("(x: %d, y: %d)\n"), point.x, point.y);
        }

        void Trace(LPCTSTR methodName, UINT keys, CPoint point)
        {
            TraceMethodName(methodName);

            CString keysText;
            if ((keys & MK_LBUTTON) != 0L)
                keysText += _T("L");
            if ((keys & MK_RBUTTON) != 0L)
                keysText += _T("R");
            TRACE(_T("%s (x: %d, y: %d)\n"), keysText.GetString(), point.x, point.y);
        }

        void TraceMethodName(LPCTSTR methodName)
        {
            TRACE(_T("TestListener - %s: "), methodName);
        }
    };
#endif // MOUSE_EVENT_TRANSLATOR_TEST

private:
    // How far (in device units) the mouse must move from the press point to count as a drag instead of a click.
    static constexpr long dragStartingDistance = 5;
    
    // The view that receives the mouse messages. Used to convert device points to logical points.
    CView&                 view;
    // Registered listeners. Not owned; they must outlive the translator.
    std::vector<Listener*> listeners;
    // The button of the press being tracked (MK_LBUTTON or MK_RBUTTON), or 0 if no button is being tracked.
    UINT                   button     = 0U;
    // True after OnDragStart was sent and until the drag ends or is aborted.
    bool                   isDragging = false;
    // Device points since the button press, kept only until the drag is recognized (then replayed and cleared).
    std::vector<CPoint>    points;

public:
    explicit MouseEventTranslator(CView& view) : view(view)
    {}

    // Registers listener. Registering the same listener again has no effect, so it never receives an event twice.
    // Returns false if it was already registered.
    bool AddListener(Listener& listener)
    {
        if (std::ranges::find(listeners, &listener) != listeners.end())
            return false;
        listeners.push_back(&listener);
        return true;
    }

    // Button down starts tracking a possible click or drag, at the given device point.
    void OnLButtonDown(UINT keys, CPoint point)
    {
        OnButtonDown(MK_LBUTTON, keys, point);
    }

    // Ends the drag, or reports a click if no drag was recognized.
    void OnLButtonUp(UINT /* keys */, CPoint point)
    {
        OnButtonUp(MK_LBUTTON, point, true);
    }

    void OnRButtonDown(UINT keys, CPoint point)
    {
        OnButtonDown(MK_RBUTTON, keys, point);
    }

    // The right button is only for dragging (panning); a right click is not a click.
    void OnRButtonUp(UINT /* keys */, CPoint point)
    {
        OnButtonUp(MK_RBUTTON, point, false);
    }

    // With the tracked button down: continues the drag, or checks whether the press has turned into one.
    // With no button down: reports the cursor position.
    // The keys passed on to the drag events are the tracked button only, even if the other button is also down.
    void OnMouseMove(UINT keys, CPoint point)
    {
        ResetIfReleased(keys);
        if (button != 0U) {
            if (isDragging)
                OnDragging(button, point);
            else
                OnDrag(button, point);
        } else if ((keys & (MK_LBUTTON | MK_RBUTTON)) == 0U) {
            OnCursor(point);
        }
        // Otherwise a button is down but its press was not seen here (e.g. pressed outside the view): ignored.
    }

    // Cancels a drag in progress when the mouse leaves the view, so the listener can discard its temporary state.
    void OnMouseLeave()
    {
        if (isDragging) {
            OnDraggingAbort();
            Clear();
        }
    }

private:
    // Converts a device point of the view to a logical point.
    CPoint DPtoLP(CPoint point)
    {
        return Geometry::DPtoLP(view, point);
    }

    // Stops tracking the current press (without notifying anyone).
    void Clear()
    {
        button     = 0U;
        isDragging = false;
        points.clear();
    }

    // Starts tracking a press of newButton, unless another button is already being tracked.
    void OnButtonDown(UINT newButton, UINT keys, CPoint point)
    {
        ResetIfReleased(keys);
        if (button != 0U)
            return; // The other button is held: keep the click or drag in progress.
        button = newButton;
        points.push_back(point);
    }

    // Ends the press of releasedButton: the drag end, or a click if isClickable and no drag was recognized.
    // The release of a button that is not being tracked is ignored.
    void OnButtonUp(UINT releasedButton, CPoint point, bool isClickable)
    {
        if (releasedButton != button)
            return;
        if (isDragging)
            OnDragEnd(button, point);
        else if (isClickable)
            OnClick(point);
        Clear();
    }

    // If the tracked button is no longer down (its release happened where it was not seen, e.g. outside the view),
    // aborts the drag in progress and stops tracking, so the next press starts cleanly.
    void ResetIfReleased(UINT keys)
    {
        if (button == 0U || (keys & button) != 0U)
            return;
        if (isDragging)
            OnDraggingAbort();
        Clear();
    }

    // Called for moves between the press and the recognition of a drag. Once the distance from the press point
    // exceeds dragStartingDistance, starts the drag at the press point and replays the buffered moves, so the
    // listener sees the whole path. Smaller moves are buffered.
    void OnDrag(UINT keys, CPoint point)
    {
        ASSERT(!points.empty());
        auto distance = Geometry::GetDistance(points.front(), point);
        if (distance > dragStartingDistance) {
            OnDragStart(keys, points.front());
            for (auto eachPoint : points | std::views::drop(1))
                OnDragging(keys, eachPoint);
            OnDragging(keys, point);

            isDragging = true;
            points.clear();
        } else {
            points.push_back(point);
        }
    }

    // Calls notify(listener) for each listener.
    void Notify(std::invocable<Listener&> auto notify)
    {
        for (auto listener : listeners)
            notify(*listener);
    }

    void OnClick(CPoint point)
    {
        const auto logicalPoint = DPtoLP(point);
        Notify([&](Listener& listener) { listener.OnClick(logicalPoint); });
    }

    void OnCursor(CPoint point)
    {
        const auto logicalPoint = DPtoLP(point);
        Notify([&](Listener& listener) { listener.OnCursor(logicalPoint); });
    }

    void OnDragStart(UINT keys, CPoint point)
    {
        const auto logicalPoint = DPtoLP(point);
        Notify([&](Listener& listener) { listener.OnDragStart(keys, logicalPoint); });
    }

    void OnDragging(UINT keys, CPoint point)
    {
        const auto logicalPoint = DPtoLP(point);
        Notify([&](Listener& listener) { listener.OnDragging(keys, logicalPoint); });
    }

    void OnDraggingAbort()
    {
        Notify([](Listener& listener) { listener.OnDraggingAbort(); });
    }

    void OnDragEnd(UINT keys, CPoint point)
    {
        const auto logicalPoint = DPtoLP(point);
        Notify([&](Listener& listener) { listener.OnDragEnd(keys, logicalPoint); });
    }
};

// Definition of the pure virtual destructor (required even though it is pure).
inline MouseEventTranslator::Listener::~Listener() {}

} // namespace Shos::MiniCadSample::Controllers
