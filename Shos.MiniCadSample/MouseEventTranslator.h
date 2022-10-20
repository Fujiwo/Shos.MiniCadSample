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

        virtual void OnInput    (CPoint /* point */) {}
        virtual void OnCursor   (CPoint /* point */) {}
        virtual void OnDragStart(UINT /* keys */, CPoint /* point */) {}
        virtual void OnDragging (UINT /* keys */, CPoint /* point */) {}
        virtual void OnDragEnd  (UINT /* keys */, CPoint /* point */) {}
    };

#ifdef MOUSE_EVENT_TRANSLATOR_TEST
    class TestListener : public MouseEventTranslator::Listener
    {
    public:
        virtual void OnInput    (CPoint point)            { Trace(_T("OnInput"    ),       point); }
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
    
    CView&              view;
    Listener&           lister;
    //CPoint              firstPoint;
    bool                hasPoint;
    bool                isDragging;
    std::vector<CPoint> points;

public:
    MouseEventTranslator(CView& view, Listener& lister) : view(view), lister(lister), hasPoint(false), isDragging(false)
    {}

    void OnLButtonDown(UINT /* keys */, CPoint point)
    {
        Clear();
        points.push_back(point);
    }

    void OnLButtonUp(UINT /* keys */, CPoint point)
    {
        if (isDragging)
            lister.OnDragEnd(MK_LBUTTON, DPtoLP(point));
        else
            lister.OnInput(DPtoLP(point));
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
            lister.OnDragEnd(MK_RBUTTON, DPtoLP(point));
        else
            lister.OnInput(DPtoLP(point));
        Clear();
    }

    void OnMouseMove(UINT keys, CPoint point)
    {
        if ((keys & MK_LBUTTON) != 0L || (keys & MK_RBUTTON) != 0L) {
            if (isDragging)
                lister.OnDragging(keys, DPtoLP(point));
            else
                OnDrag(keys, point);
        } else {
            lister.OnCursor(DPtoLP(point));
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
            lister.OnDragStart(keys, DPtoLP(points[0]));
            for (size_t index = 1; index < points.size(); index++)
                lister.OnDragging(keys, DPtoLP(points[index]));
            lister.OnDragging(keys, DPtoLP(point));

            isDragging = true;
            points.clear();
        } else {
            points.push_back(point);
        }
    }
};

inline MouseEventTranslator::Listener::~Listener() {}
