#pragma once
//#define SCROLL_VIEW
#define ZOOMING_VIEW

#include "DoubleBuffer.h"
#include "ClipboardHelper.h"
#ifdef ZOOMING_VIEW
#include "Zooming.h"
#endif // ZOOMING_VIEW
#include "MainFrame.h"
#include "MouseEventTranslator.h"

class View : public
#ifdef SCROLL_VIEW
    DoubleBufferScrollView
#else // SCROLL_VIEW
    DoubleBufferView
#endif // SCROLL_VIEW 
{
#ifdef ZOOMING_VIEW
    Zooming zooming;
#endif // ZOOMING_VIEW


#ifdef MOUSE_EVENT_TRANSLATOR_TEST
    MouseEventTranslator::TestListener testListener;
#endif // MOUSE_EVENT_TRANSLATOR_TEST
    MouseEventTranslator mouseEventTranslator;
    
public:
    View() : mouseEventTranslator(*this)
#ifdef ZOOMING_VIEW
    , zooming(*this, Document::GetMinimumSize(), Document::GetArea())
#endif // ZOOMING_VIEW
    {
        SetBackgroundColor(GetBackgroundColor());
    }
    
protected:
    Document& GetDocument() const
    {
        ASSERT_VALID(m_pDocument);
        return reinterpret_cast<Document&>(*m_pDocument);
    }

    virtual void View::OnInitialUpdate() override
    {
#ifdef SCROLL_VIEW
        DoubleBufferScrollView::OnInitialUpdate();
        SetScrollSizes(MM_TEXT, GetDocument().GetSize());
#endif // SCROLL_VIEW 

        Application::Set(GetDocument().GetCurrentFigureAttribute());
        Application::SetFigureAttributeObserver(GetDocument().GetModel());

#ifdef MOUSE_EVENT_TRANSLATOR_TEST
        mouseEventTranslator.AddListener(testListener);
#endif // MOUSE_EVENT_TRANSLATOR_TEST
        mouseEventTranslator.AddListener(GetDocument());
    }

#ifndef SCROLL_VIEW
    virtual void OnPrepareDC(CDC* dc, CPrintInfo* pInfo = nullptr) override
    {
        DoubleBufferView::OnPrepareDC(dc, pInfo);
        ASSERT_VALID(dc);

#ifdef ZOOMING_VIEW
        zooming.PrepareDC(*dc);
#else // ZOOMING_VIEW
#ifndef SCROLL_VIEW
        PrepareDC(*dc);
#endif // SCROLL_VIEW
#endif // ZOOMING_VIEW
    }

#ifndef ZOOMING_VIEW
#ifndef SCROLL_VIEW
    void PrepareDC(CDC& dc)
    {
        dc.SetMapMode(MM_ISOTROPIC);

        auto documentArea = GetDocument().GetArea();
        dc.SetWindowOrg(documentArea.CenterPoint());
        dc.SetWindowExt(documentArea.Size());

        CRect clientRect;
        GetClientRect(clientRect);
        dc.SetViewportOrg(clientRect.CenterPoint());
        dc.SetViewportExt(clientRect.Size());
    }
#endif // SCROLL_VIEW
#endif // ZOOMING_VIEW

#endif // SCROLL_VIEW
    virtual void OnDrawLayer1(CDC& dc)
    {
        GetDocument().DrawArea(dc);
        DrawFigures(dc, GetDocument());
    }

    virtual void OnDrawLayer2(CDC& dc)
    {
        GetDocument().DrawCommand(dc);
    }

    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override
    {
        Update();

        if (pHint == nullptr || static_cast<const Hint*>(pHint)->type == Hint::Type::All || static_cast<const Hint*>(pHint)->type == Hint::Type::ViewOnly) {
            #ifdef SCROLL_VIEW
            DoubleBufferScrollView
            #else // SCROLL_VIEW
            DoubleBufferView
            #endif // SCROLL_VIEW 
                                  ::OnUpdate(pSender, lHint, pHint);
            return;
        }

        CRect area;
        FigureHelper::GetArea(static_cast<const Hint*>(pHint)->figures, area);
        
        TRACE(_T("View::OnUpdate: area   (top: %d, left: %d, width: %d, height: %d)\n"), area.top, area.left, area.Width(), area.Height());
        area = LPtoDP(area);
        TRACE(_T("View::OnUpdate: clipbox(top: %d, left: %d, width: %d, height: %d)\n"), area.top, area.left, area.Width(), area.Height());

        InvalidateRect(area);
    }

    afx_msg void OnEditCopy()
    {
        ClipboardHelper::OnEditCopy(GetDocument(), *this, GetDocument().GetSize(), GetBackgroundColor(), [&](CDC& dc) { GetDocument().Draw(dc); });
    }

    afx_msg void OnEditCut()
    {
        ClipboardHelper::OnEditCut(GetDocument(), *this, GetDocument().GetSize(), GetBackgroundColor(), [&](CDC& dc) { GetDocument().Draw(dc); });
    }

    afx_msg void OnEditPaste()
    {
        ClipboardHelper::OnEditPaste(GetDocument(), *this);
    }

    afx_msg void OnLButtonDown(UINT keys, CPoint point)
    {
        mouseEventTranslator.OnLButtonDown(keys, point);
    }

    afx_msg void OnLButtonUp(UINT keys, CPoint point)
    {
        mouseEventTranslator.OnLButtonUp(keys, point);
    }

    afx_msg void OnRButtonDown(UINT keys, CPoint point)
    {
        mouseEventTranslator.OnRButtonDown(keys, point);
    }

    afx_msg void OnRButtonUp(UINT keys, CPoint point)
    {
        mouseEventTranslator.OnRButtonUp(keys, point);
    }

    afx_msg void OnMouseMove(UINT keys, CPoint point)
    {
        TrackMouseLeaveEvent();
        mouseEventTranslator.OnMouseMove(keys, point);
        Invalidate();
    }
    
    afx_msg LRESULT OnMouseLeave(WPARAM /* wParam */, LPARAM /* lParam */)
    {
        mouseEventTranslator.OnMouseLeave();
        return 0L;
    }

#ifdef ZOOMING_VIEW
    afx_msg BOOL OnMouseWheel(UINT keys, short delta, CPoint point)
    {
        if (zooming.OnMouseWheel(keys, delta, point)) {
            Update();
            Invalidate();
        }
        return DoubleBufferView::OnMouseWheel(keys, delta, point);
    }
#endif // ZOOMING_VIEW

    afx_msg void OnDestroyClipboard()
    {
        ClipboardHelper::OnDestroyClipboard();
    }

private:
    void TrackMouseLeaveEvent() const
    {
        TRACKMOUSEEVENT trackmouseevent;
        trackmouseevent.cbSize      = sizeof(trackmouseevent);
        trackmouseevent.dwFlags     = TME_LEAVE;
        trackmouseevent.hwndTrack   = GetSafeHwnd();
        trackmouseevent.dwHoverTime = HOVER_DEFAULT;
        ::TrackMouseEvent(&trackmouseevent);
    }

    static COLORREF GetBackgroundColor()
    {
        return ::GetSysColor(COLOR_WINDOW);
    }
    
    void DrawFigures(CDC& dc, const Document& document)
    {
        CRect clipBox;
        auto clippingMode = dc.GetClipBox(clipBox);

        if (clippingMode == SIMPLEREGION || clippingMode == COMPLEXREGION) {
            dc.DPtoLP(clipBox);
            for (auto figure : document) {
                ASSERT_VALID(figure);
                if (HasIntersection(*figure, clipBox))
                    figure->Draw(dc);
            }
        }
        else {
            ASSERT(false);
            //document.Draw(dc);
        }
    }

    bool HasIntersection(const Figure& figure, const CRect& clipBox)
    {
        CRect intersection;
        return intersection.IntersectRect(figure.GetArea(), clipBox);
    }

    CPoint DPtoLP(CPoint point)
    {
        return Geometry::DPtoLP(*this, point);
    }

    CRect LPtoDP(CRect rect)
    {
        return Geometry::LPtoDP(*this, rect);
    }

    DECLARE_DYNCREATE(View)
    DECLARE_MESSAGE_MAP()
};
