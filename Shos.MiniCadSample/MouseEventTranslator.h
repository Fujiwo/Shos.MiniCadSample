#pragma once

//#define MOUSE_EVENT_TRANSLATOR_TEST

#include <afx.h>
#include <vector>

class MouseEventTranslator
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = 0;

        virtual void OnClick    (CPoint /* point */) {}
        virtual void OnCursor   (CPoint /* point */) {}
        virtual void OnDragStart(UINT /* keys */, CPoint /* point */) {}
        virtual void OnDragging (UINT /* keys */, CPoint /* point */) {}
        virtual void OnDragEnd  (UINT /* keys */, CPoint /* point */) {}
    };

#ifdef MOUSE_EVENT_TRANSLATOR_TEST
    class TestListener : public MouseEventTranslator::Listener
    {
    public:
        virtual void OnClick    (CPoint point)            { Trace(_T("OnClick"    ),       point); }
        virtual void OnCursor   (CPoint point)            { Trace(_T("OnCursor"   ),       point); }
        virtual void OnDragStart(UINT keys, CPoint point) { Trace(_T("OnDragStart"), keys, point); }
        virtual void OnDragging (UINT keys, CPoint point) { Trace(_T("OnDragging" ), keys, point); }
        virtual void OnDragEnd  (UINT keys, CPoint point) { Trace(_T("OnDragEnd"  ), keys, point); }

    private:
        void Trace(LPCTSTR methodName, CPoint point)
        {
            TRACE(_T("TestListener - %s: (x: %d, y: %d)\n"), methodName, point.x, point.y);
        }

        void Trace(LPCTSTR methodName, UINT keys, CPoint point)
        {
            CString keysText;
            if ((keys & MK_LBUTTON) != 0L)
                keysText += _T("L");
            if ((keys & MK_RBUTTON) != 0L)
                keysText += _T("R");
            TRACE(_T("TestListener - %s: %s (x: %d, y: %d)\n"), methodName, keysText.GetString(), point.x, point.y);
        }
    };
#endif // MOUSE_EVENT_TRANSLATOR_TEST

private:
    static const long dragStartingDistance = 5;
    
    CView&                 view;
    std::vector<Listener*> listeners;
    bool                   isDragging;
    std::vector<CPoint>    points;

public:
    MouseEventTranslator(CView& view) : view(view), isDragging(false)
    {}

    void AddListener(Listener& listener)
    {
        listeners.push_back(&listener);
    }

    void OnLButtonDown(UINT /* keys */, CPoint point)
    {
        Clear();
        points.push_back(point);
    }

    void OnLButtonUp(UINT /* keys */, CPoint point)
    {
        if (isDragging)
            OnDragEnd(MK_LBUTTON, point);
        else
            OnClick(point);
        Clear();
    }

    void OnRButtonDown(UINT /* keys */, CPoint point)
    {
        Clear();
        points.push_back(point);
    }

    void OnRButtonUp(UINT /* keys */, CPoint point)
    {
        if (isDragging)
            OnDragEnd(MK_RBUTTON, point);
        else
            OnClick(point);
        Clear();
    }

    void OnMouseMove(UINT keys, CPoint point)
    {
        if ((keys & MK_LBUTTON) != 0L || (keys & MK_RBUTTON) != 0L) {
            if (isDragging)
                OnDragging(keys, point);
            else
                OnDrag(keys, point);
        } else {
            OnCursor(point);
        }
    }

private:
    CPoint DPtoLP(CPoint point)
    {
        return Geometry::DPtoLP(view, point);
    }

    void Clear()
    {
        isDragging = false;
        points.clear();
    }

    void OnDrag(UINT keys, CPoint point)
    {
        ASSERT(points.size() > 0);
        auto distance = Geometry::GetDistance(points[0], point);
        if (distance > dragStartingDistance) {
            OnDragStart(keys, points[0]);
            for (size_t index = 1; index < points.size(); index++)
                OnDragging(keys, points[index]);
            OnDragging(keys, point);

            isDragging = true;
            points.clear();
        } else {
            points.push_back(point);
        }
    }

    void OnClick(CPoint point)
    {
        auto logicalPoint = DPtoLP(point);
        std::for_each(listeners.begin(), listeners.end(), [&](Listener* listener) { listener->OnClick(logicalPoint); });
    }

    void OnCursor(CPoint point)
    {
        auto logicalPoint = DPtoLP(point);
        std::for_each(listeners.begin(), listeners.end(), [&](Listener* listener) { listener->OnCursor(logicalPoint); });
    }

    void OnDragStart(UINT keys, CPoint point)
    {
        auto logicalPoint = DPtoLP(point);
        std::for_each(listeners.begin(), listeners.end(), [&](Listener* listener) { listener->OnDragStart(keys, logicalPoint); });
    }

    void OnDragging(UINT keys, CPoint point)
    {
        auto logicalPoint = DPtoLP(point);
        std::for_each(listeners.begin(), listeners.end(), [&](Listener* listener) { listener->OnDragging(keys, logicalPoint); });
    }

    void OnDragEnd(UINT keys, CPoint point)
    {
        auto logicalPoint = DPtoLP(point);
        std::for_each(listeners.begin(), listeners.end(), [&](Listener* listener) { listener->OnDragEnd(keys, logicalPoint); });
    }
};

inline MouseEventTranslator::Listener::~Listener() {}
