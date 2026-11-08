#pragma once

// The view of the document: draws the figures and the active command, turns window messages into
// mouse events for the controllers, handles clipboard commands, and (optionally) zooms and pans.

// Build options. Define at most one of the two: SCROLL_VIEW derives from the scroll-view base class
// (scroll bars, no zoom); ZOOMING_VIEW derives from the plain view and zooms/pans with the mouse (see Zooming.h).
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

// Shows a Document. It is also a MouseEventTranslator listener (besides Document), used for panning.
// Logical coordinates are those of the model; device coordinates are those of the client area.
// OnPrepareDC sets the mapping between them, so figures are drawn in logical coordinates.
class View : public VIEW_BASE, public MouseEventTranslator::Listener
{
    using Base = VIEW_BASE;
#undef VIEW_BASE

#ifdef ZOOMING_VIEW
    Zooming zooming; // The visible part of the drawing area (zoom and pan).
#endif // ZOOMING_VIEW


#ifdef MOUSE_EVENT_TRANSLATOR_TEST
    MouseEventTranslator::TestListener testListener;
#endif // MOUSE_EVENT_TRANSLATOR_TEST
    MouseEventTranslator mouseEventTranslator; // Converts raw mouse messages into click / drag events.

    // OnInitialUpdate runs again on every File > New / Open (the SDI document and view are reused),
    // so the one-time registrations below are guarded by this flag.
    bool isInitialized = false;
    
public:
    // The zooming range is the whole document area; it cannot be zoomed in beyond the minimum size.
    View() : mouseEventTranslator(*this)
#ifdef ZOOMING_VIEW
    , zooming(*this, Document::GetMinimumSize(), Document::GetArea())
#endif // ZOOMING_VIEW
    {
        SetBackgroundColor(GetBackgroundColor());
    }
    
protected:
    // The document is always a Document (the SDI template creates it).
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
    // Sets the mapping from logical coordinates (model) to device coordinates (client area).
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
    // Fixed mapping that fits the whole document area into the client area (no zoom).
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
    // Layer 1 (cached): the drawing area and the figures. Rebuilt only after Update().
    virtual void OnDrawLayer1(CDC& dc) override
    {
        GetDocument().DrawArea(dc);
        DrawFigures(dc, GetDocument());
    }

    // Layer 2 (drawn on every WM_PAINT): the rubber band and cursor of the active command.
    virtual void OnDrawLayer2(CDC& dc) override
    {
        GetDocument().DrawCommand(dc);
    }

    // Called when the model changes (through the document). Discards the cached layer 1, and
    // invalidates only the changed figures' area unless the whole view must be redrawn (hint is null, All or ViewOnly).
    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override
    {
        Update();

        const auto hint = static_cast<const Hint*>(pHint);
        if (hint == nullptr || hint->type == Hint::Type::All || hint->type == Hint::Type::ViewOnly) {
            Base::OnUpdate(pSender, lHint, pHint);
            return;
        }

        CRect area;
        // The area is in logical coordinates; InvalidateRect needs device coordinates.
        FigureHelper::GetArea(hint->figures, area);
        
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

    // Stops panning where the view is.
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

    // Mouse messages are forwarded to the translator, which calls back the listeners
    // (Document for the commands, and this view for panning).
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
        Invalidate(); // Redraws layer 2 (the cursor and the rubber band follow the mouse); layer 1 stays cached.
    }
    
    // WM_MOUSELEAVE: requested by TrackMouseLeaveEvent. Lets the translator hide the cursor and so on.
    afx_msg LRESULT OnMouseLeave(WPARAM /* wParam */, LPARAM /* lParam */)
    {
        mouseEventTranslator.OnMouseLeave();
        return 0L;
    }

#ifdef ZOOMING_VIEW
    // Ctrl + wheel zooms around the mouse position (see Zooming::OnMouseWheel).
    afx_msg BOOL OnMouseWheel(UINT keys, short delta, CPoint point)
    {
        if (zooming.OnMouseWheel(keys, delta, point)) {
            Update();
            Invalidate();
        }
        return Base::OnMouseWheel(keys, delta, point);
    }
#endif // ZOOMING_VIEW

    // The Delete key removes the selected figures (undoable).
    afx_msg void OnKeyDown(UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */)
    {
        if (nChar == VK_DELETE)
            GetDocument().RemoveSelectedFigures();
    }

private:
    // Asks Windows to send WM_MOUSELEAVE once when the mouse leaves the client area (the request is one-shot).
    void TrackMouseLeaveEvent() const
    {
        TRACKMOUSEEVENT trackmouseevent{
            .cbSize      = sizeof(TRACKMOUSEEVENT),
            .dwFlags     = TME_LEAVE,
            .hwndTrack   = GetSafeHwnd(),
            .dwHoverTime = HOVER_DEFAULT
        };
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

    // Also used as the background of the images put on the clipboard.
    static COLORREF GetBackgroundColor()
    {
        return ::GetSysColor(COLOR_WINDOW);
    }
    
    // Draws the figures that intersect the clip box of dc (a speed-up for partial redraws).
    // dc must have the logical-coordinate mapping set, and a clip box.
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
            ASSERT(false); // A clip box is always expected.
            //document.Draw(dc);
        }
    }

    static bool HasIntersection(const Figure& figure, const CRect& clipBox)
    {
        CRect intersection;
        return intersection.IntersectRect(figure.GetArea(), clipBox);
    }

    // Device (client) to logical coordinates, and the reverse, with the current mapping of this view.
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
