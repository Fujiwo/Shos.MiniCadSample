#pragma once
//#define SCROLL_VIEW
#define ZOOMING_VIEW

#include "DoubleBuffer.h"
#include "ClipboardHelper.h"
#ifdef ZOOMING_VIEW
#include "Zooming.h"
#endif // ZOOMING_VIEW
#include "MainFrame.h"
#include "Document.h"
#include "MouseEventTranslator.h"

namespace Shos::MiniCadSample::Views {

using namespace Models;
using namespace Utilities;
using namespace Controllers;

// The base class. An alias in View, not "using namespace" here, so that the namespace does not leak
// into every file that includes this header.
#ifdef SCROLL_VIEW
#define VIEW_BASE Shos::MFCDoubleBuffer::DoubleBufferScrollView
#else // SCROLL_VIEW
#define VIEW_BASE Shos::MFCDoubleBuffer::DoubleBufferView
#endif // SCROLL_VIEW

class View : public VIEW_BASE, public MouseEventTranslator::Listener
{
    using Base = VIEW_BASE;
#undef VIEW_BASE

#ifdef ZOOMING_VIEW
    Zooming zooming;
#endif // ZOOMING_VIEW


#ifdef MOUSE_EVENT_TRANSLATOR_TEST
    MouseEventTranslator::TestListener testListener;
#endif // MOUSE_EVENT_TRANSLATOR_TEST
    MouseEventTranslator mouseEventTranslator;

    // OnInitialUpdate runs again on every File > New / Open (the SDI document and view are reused),
    // so the one-time registrations below are guarded by this flag.
    bool isInitialized = false;
    
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

    virtual void OnInitialUpdate() override
    {
#ifdef SCROLL_VIEW
        Base::OnInitialUpdate();
        SetScrollSizes(MM_TEXT, GetDocument().GetSize());
#endif // SCROLL_VIEW 

        // Shows the attribute of the (new) document in the dialog every time.
        Application::Set(GetDocument().GetCurrentFigureAttribute());

        // The document object is reused by the SDI template, so registering once is enough.
        // Registering again would deliver every event / attribute change twice.
        if (isInitialized)
            return;
        isInitialized = true;

        Application::SetFigureAttributeObserver(GetDocument().GetModel());

#ifdef MOUSE_EVENT_TRANSLATOR_TEST
        mouseEventTranslator.AddListener(testListener);
#endif // MOUSE_EVENT_TRANSLATOR_TEST
        mouseEventTranslator.AddListener(GetDocument());
        mouseEventTranslator.AddListener(*this);
    }

#ifndef SCROLL_VIEW
    virtual void OnPrepareDC(CDC* dc, CPrintInfo* pInfo = nullptr) override
    {
        Base::OnPrepareDC(dc, pInfo);
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
    virtual void OnDrawLayer1(CDC& dc) override
    {
        GetDocument().DrawArea(dc);
        DrawFigures(dc, GetDocument());
    }

    virtual void OnDrawLayer2(CDC& dc) override
    {
        GetDocument().DrawCommand(dc);
    }

    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override
    {
        Update();

        if (pHint == nullptr || static_cast<const Hint*>(pHint)->type == Hint::Type::All || static_cast<const Hint*>(pHint)->type == Hint::Type::ViewOnly) {
            Base::OnUpdate(pSender, lHint, pHint);
            return;
        }

        CRect area;
        FigureHelper::GetArea(static_cast<const Hint*>(pHint)->figures, area);
        
        TRACE(_T("View::OnUpdate: area   (top: %d, left: %d, width: %d, height: %d)\n"), area.top, area.left, area.Width(), area.Height());
        area = LPtoDP(area);
        TRACE(_T("View::OnUpdate: clipbox(top: %d, left: %d, width: %d, height: %d)\n"), area.top, area.left, area.Width(), area.Height());

        InvalidateRect(area);
    }

#ifdef ZOOMING_VIEW
    // Panning by dragging with the right button.
    virtual void OnDragStart(UINT keys, CPoint point) override
    {
        if ((keys & MK_RBUTTON) != 0U)
            zooming.OnDragStart(point);
    }
    
    // The view follows the mouse while dragging.
    virtual void OnDragging(UINT keys, CPoint point) override
    {
        if ((keys & MK_RBUTTON) != 0U && zooming.OnDragging(point)) {
            Update();
            Invalidate();
        }
    }

    virtual void OnDraggingAbort() override
    {
        zooming.OnDraggingAbort();
    }

    virtual void OnDragEnd(UINT keys, CPoint point) override
    {
        if ((keys & MK_RBUTTON) != 0U && zooming.OnDragEnd(point)) {
            Update();
            Invalidate();
        }
    }
#endif // ZOOMING_VIEW

    // Copy: the selected figures, or all of them if none is selected.
    afx_msg void OnEditCopy()
    {
        CopyFigures();
    }

    afx_msg void OnUpdateEditCopy(CCmdUI* cmdUI)
    {
        cmdUI->Enable(GetDocument().GetModel().IsEmpty() ? 0 : 1);
    }

    // Cut: copies the selected figures, then deletes them (undoable). Disabled without a selection.
    afx_msg void OnEditCut()
    {
        if (!GetDocument().GetModel().HasSelection())
            return;
        if (CopyFigures())
            GetDocument().RemoveSelectedFigures();
    }

    afx_msg void OnUpdateEditCut(CCmdUI* cmdUI)
    {
        cmdUI->Enable(GetDocument().GetModel().HasSelection() ? 1 : 0);
    }

    // Paste: adds the figures on the clipboard as one undo step and selects them.
    // The model notifies the views, which redraw and set the modified flag.
    afx_msg void OnEditPaste()
    {
        if (!ClipboardHelper::Paste(*this, [&](CArchive& ar) { GetDocument().Paste(ar); }))
            ::MessageBeep(MB_ICONWARNING);
    }

    afx_msg void OnUpdateEditPaste(CCmdUI* cmdUI)
    {
        cmdUI->Enable(ClipboardHelper::CanPaste() ? 1 : 0);
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
        return Base::OnMouseWheel(keys, delta, point);
    }
#endif // ZOOMING_VIEW

    afx_msg void OnKeyDown(UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */)
    {
        if (nChar == VK_DELETE)
            GetDocument().RemoveSelectedFigures();
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

    // Copies the selected figures, or all of them (with the drawing area) if none is selected.
    bool CopyFigures()
    {
        const auto& model        = GetDocument().GetModel();
        const bool  selectedOnly = model.HasSelection();
        // The selected figures are output in their own bounding rectangle; all the figures in the whole document area.
        const auto  area         = selectedOnly ? model.GetFiguresArea(true) : Document::GetArea();

        return ClipboardHelper::Copy(*this, area, GetBackgroundColor(),
            [&](CArchive& ar) { model.StoreFigures(ar, selectedOnly); },
            [&](CDC& dc) {
                if (!selectedOnly)
                    GetDocument().DrawArea(dc);
                model.DrawFigures(dc, selectedOnly);
            });
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

} // namespace Shos::MiniCadSample::Views
