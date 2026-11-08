// DoubleBuffer.h : A double-buffering library for MFC views.
//
// Role: lets a view split its drawing into a rarely changing layer (cached in a bitmap)
// and a frequently changing layer (drawn on top at every paint), without flicker.
//
// How it works
//   The drawing of a view is handled as two layers.
//
//   - Layer 1 (OnDrawLayer1): content that changes rarely (for example, committed figures).
//       It is drawn into a bitmap (the cache) that has the size of the client area, and is kept there.
//       It is not drawn again until the cache is invalidated by Update().
//   - Layer 2 (OnDrawLayer2): content that changes often (for example, a rubber band or a cursor mark).
//       It is drawn on top of the cache at every WM_PAINT.
//
//   WM_PAINT shows the view on the screen in the following steps.
//     1. If the cache is invalid, fill it with the background color, draw Layer 1 and rebuild the cache.
//     2. Copy the cache to a work bitmap and draw Layer 2 on it.
//     3. Copy the work bitmap to the screen in one step (no intermediate result is visible, so there is no flicker).
//   Steps 1 and 2 handle only the area that needs to be repainted (the clip box).
//   The cache and the work bitmap are recreated only when the size of the client area changes.
//   If a bitmap cannot be created (for example, out of memory), the view is drawn directly to the screen
//   (it may flicker, but the display is correct).
//
// Usage
//   Derive the view from DoubleBufferView (CView version) or DoubleBufferScrollView (CScrollView version),
//   and override OnDrawLayer1 / OnDrawLayer2 instead of OnDraw.
//
// Limitations
//   Printing and print preview are not supported.

#pragma once

#include <concepts>
#include <optional>
#include "GdiObjectSelector.h"

namespace Shos::MFCDoubleBuffer {

// The core of double buffering: owns the cache and work bitmaps and implements the painting steps.
// It is used through multiple inheritance with a view class (do not use it directly; use DoubleBufferView / DoubleBufferScrollView).
class DoubleBuffer
{
    bool     isCacheValid    = false;                       // Whether the content of the cache is up to date.
    CBitmap  cache;                                         // Bitmap that holds Layer 1 (has the size of the client area).
    CSize    cacheSize;                                     // Size of cache.
    CBitmap  work;                                          // Work bitmap in which Layer 1 and Layer 2 are combined (same size as cache).
    CSize    workSize;                                      // Size of work.
    CPoint   cacheOrigin;                                   // Scroll position (device coordinates) at which the cache was built.
    COLORREF backgroundColor = ::GetSysColor(COLOR_WINDOW); // Color used to fill the cache before Layer 1 is drawn.
    CView&   view;                                          // The view to draw (the class that derives from this object).

public:
    // Binds the buffer to the view that owns it (the view must outlive this object).
    explicit DoubleBuffer(CView& view) : view(view)
    {}

    // Returns the view to draw (in a Debug build, checks that its window has been created).
    [[nodiscard]] CView& Window()
    {
        ASSERT_VALID(&view);
        ASSERT(view.GetSafeHwnd() != nullptr && ::IsWindow(view.GetSafeHwnd()));
        return view;
    }

protected:
    // A DC for drawing directly into the cache (Layer 1).
    //
    // Use it when the content of Layer 1 grows only a little (for example, one figure is added): instead of
    // rebuilding the whole cache with Update(), draw only the difference.
    // After drawing, make the affected area repaint with InvalidateRect(..., FALSE).
    //
    // OnPrepareDC has already been applied, so you can draw in logical coordinates (which take the scroll position into account).
    // Content drawn while the cache is invalid (for example, after Update()) is discarded when the cache is rebuilt next time.
    //
    // Precondition: the cache bitmap must already exist (after the first WM_PAINT).
    // Before that there is no bitmap to select, so the drawing is lost (detected by ASSERT in a Debug build).
    //
    //   Example:
    //     DrawFigure(CacheDevice(*this), point);  // used as a temporary object; cleaned up at the end of the statement
    class CacheDevice final
    {
        CView&                           view;
        CDC                              memoryDC;    // Memory DC with the cache bitmap selected into it.
        std::optional<GdiObjectSelector> selector;    // Constructed later, because the bitmap is selected only after memoryDC is created.
        int                              savedDC = 0; // State of the DC before OnPrepareDC was applied (restored by the destructor).

    public:
        explicit CacheDevice(DoubleBuffer& doubleBuffer) : view(doubleBuffer.Window())
        {
            ASSERT(doubleBuffer.cache.GetSafeHandle() != nullptr); // Cannot be used before the first WM_PAINT.
            CClientDC clientDC(&view);
            memoryDC.CreateCompatibleDC(&clientDC);
            selector.emplace(memoryDC, doubleBuffer.cache);

            // Set the scroll position and mapping mode in the same way as for the screen.
            savedDC = memoryDC.SaveDC();
            view.OnPrepareDC(&memoryDC);
        }

        ~CacheDevice()
        { memoryDC.RestoreDC(savedDC); }

        // Copying is forbidden so that the DC and the bitmap selection are not released twice.
        CacheDevice(const CacheDevice&)            = delete;
        CacheDevice& operator=(const CacheDevice&) = delete;

        // Lets the object be passed as is to drawing functions that take a CDC&.
        operator CDC& ()
        { return memoryDC; }
    };

    // Changes the background color (the cache is rebuilt at the next paint).
    void SetBackgroundColor(COLORREF newBackgroundColor)
    {
        backgroundColor = newBackgroundColor;
        Update();
    }

    // Invalidates the cache.
    // Call it when the content of Layer 1 has changed. The cache is actually rebuilt at the next WM_PAINT.
    // To reflect the change on the screen immediately, call Invalidate(FALSE) afterwards.
    void Update()
    { isCacheValid = false; }

    // Draws Layer 1 and Layer 2 on top of each other to the WM_PAINT DC (CPaintDC).
    void Draw(CDC& dc)
    {
        CRect clientRect;
        view.GetClientRect(&clientRect);
        if (clientRect.IsRectEmpty()) // There is nowhere to draw (for example, the window is minimized).
            return;

        // The area that needs repainting. OnPrepareDC has not been applied yet, so it is in device coordinates.
        CRect clipBox;
        dc.GetClipBox(&clipBox);
        clipBox &= clientRect;
        if (clipBox.IsRectEmpty())
            return;

        // The cache holds only the visible part of the view, so it depends on the scroll position.
        // If the scroll position has changed, rebuild it.
        // Normal scrolling is already handled by OnScrollBy; we get here only when ScrollToPosition or similar is called directly.
        if (GetCacheOrigin() != cacheOrigin) {
            Update();
            // Parts that were only moved by ScrollWindow still contain the old Layer 2, so make the whole area repaint.
            if (clipBox != clientRect)
                view.Invalidate(FALSE);
        }
        if (!UpdateCache(dc, clientRect) || !PrepareBitmap(work, workSize, dc, clientRect.Size())) {
            DrawDirectly(dc, clipBox);
            return;
        }

        CDC cacheDC;
        cacheDC.CreateCompatibleDC(&dc);
        GdiObjectSelector cacheSelector(cacheDC, cache);

        // The work bitmap has the size of the client area, like the cache (client coordinates are used as they are).
        CDC workDC;
        workDC.CreateCompatibleDC(&dc);
        GdiObjectSelector workSelector(workDC, work);

        const auto [width, height] = clipBox.Size();

        // 1. Copy the part of the cache (Layer 1) that needs repainting to the work bitmap.
        workDC.BitBlt(clipBox.left, clipBox.top, width, height, &cacheDC, clipBox.left, clipBox.top, SRCCOPY);

        // 2. Draw Layer 2 on it (never outside the area that needs repainting).
        const auto savedDC = workDC.SaveDC();
        workDC.IntersectClipRect(&clipBox); // OnPrepareDC has not been applied yet, so the clip rectangle can be given in device coordinates.
        view.OnPrepareDC(&workDC);
        OnDrawLayer2(workDC);
        workDC.RestoreDC(savedDC);

        // 3. Transfer the combined result to the screen in one step.
        dc.BitBlt(clipBox.left, clipBox.top, width, height, &workDC, clipBox.left, clipBox.top, SRCCOPY);
    }

    // Returns the scroll position (device coordinates) on which the content of the cache depends.
    // Always (0, 0) for a view that does not scroll. Overridden in DoubleBufferScrollView.
    [[nodiscard]] virtual CPoint GetCacheOrigin() const
    { return {}; }

    // Draws Layer 1 (the cached layer). Override in a derived class.
    // The DC has had OnPrepareDC applied, so draw in logical coordinates.
    virtual void OnDrawLayer1(CDC&) {}

    // Draws Layer 2 (the layer drawn at every paint). Override in a derived class.
    // The DC has had OnPrepareDC applied, so draw in logical coordinates.
    virtual void OnDrawLayer2(CDC&) {}

private:
    // Rebuilds the cache if it is invalid.
    // Returns true if the cache is usable (false if a bitmap could not be created).
    [[nodiscard]] bool UpdateCache(CDC& dc, const CRect& clientRect)
    {
        if (isCacheValid)
            return true;

        // If the size of the client area has changed, the bitmap is recreated.
        if (!PrepareBitmap(cache, cacheSize, dc, clientRect.Size()))
            return false;

        CDC memoryDC;
        memoryDC.CreateCompatibleDC(&dc);
        GdiObjectSelector selector(memoryDC, cache);

        // Fill the background (done in device coordinates, so before OnPrepareDC).
        memoryDC.FillSolidRect(&clientRect, backgroundColor);

        // Draw Layer 1 in logical coordinates.
        const auto savedDC = memoryDC.SaveDC();
        view.OnPrepareDC(&memoryDC);
        OnDrawLayer1(memoryDC);
        memoryDC.RestoreDC(savedDC);

        cacheOrigin  = GetCacheOrigin();
        isCacheValid = true;
        return true;
    }

    // If bitmap has already been created with the given size, use it as is. Otherwise recreate it.
    // Returns true if bitmap is usable (false if it could not be created; bitmap is then empty).
    [[nodiscard]] static bool PrepareBitmap(CBitmap& bitmap, CSize& bitmapSize, CDC& dc, CSize size)
    {
        if (bitmap.GetSafeHandle() != nullptr && bitmapSize == size)
            return true;

        bitmap.DeleteObject();
        bitmapSize = CSize();
        if (!bitmap.CreateCompatibleBitmap(&dc, size.cx, size.cy))
            return false;
        bitmapSize = size;
        return true;
    }

    // Fallback when a bitmap could not be created: draws directly to the screen DC.
    void DrawDirectly(CDC& dc, const CRect& clipBox)
    {
        dc.FillSolidRect(&clipBox, backgroundColor); // The background was not erased by OnEraseBkgnd, so erase it here.

        const auto savedDC = dc.SaveDC();
        view.OnPrepareDC(&dc);
        OnDrawLayer1(dc);
        OnDrawLayer2(dc);
        dc.RestoreDC(savedDC);
    }
};

// Common base class that adds double buffering to a view class (CView or a class derived from it).
// An MFC message map cannot be defined in a template, so only the message handler implementations are here;
// the maps are defined in the derived classes (in DoubleBuffer.cpp).
template <std::derived_from<CView> TView>
class DoubleBufferViewBase : public TView, protected DoubleBuffer
{
public:
    // Base classes are constructed in declaration order (TView, then DoubleBuffer),
    // so the TView part has already been constructed when DoubleBuffer is constructed.
    DoubleBufferViewBase() : DoubleBuffer(static_cast<TView&>(*this))
    {}

protected:
    // Painting for WM_PAINT.
    // Override this to add processing before or after drawing.
    virtual void OnPaintBuffered(CDC& dc)
    { Draw(dc); }

    // Drawing is done by OnPaint -> Draw, so OnDraw is not used
    // (it is a pure virtual function of CView, so a definition is required; printing is not supported).
    void OnDraw(CDC*) override
    { ASSERT(false); }

    // When the document is updated (UpdateAllViews and so on), rebuild Layer 1.
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override
    {
        Update();
        TView::OnUpdate(pSender, lHint, pHint);
    }

    afx_msg void OnPaint()
    {
        CPaintDC dc(this);
        OnPaintBuffered(dc);
    }

    // The background is drawn in Draw, so it is not erased here (erasing would cause flicker).
    afx_msg BOOL OnEraseBkgnd(CDC*)
    { return TRUE; }

    // When the size of the client area changes, the cache bitmap is recreated.
    afx_msg void OnSize(UINT nType, int cx, int cy)
    {
        TView::OnSize(nType, cx, cy);
        Update();
    }
};

// A CView with double buffering.
class DoubleBufferView : public DoubleBufferViewBase<CView>
{
    DECLARE_DYNCREATE(DoubleBufferView)
    DECLARE_MESSAGE_MAP()
};

// A CScrollView with double buffering.
// The cache holds only the visible part, so it is rebuilt at every scroll.
class DoubleBufferScrollView : public DoubleBufferViewBase<CScrollView>
{
protected:
    // All scrolling by scroll bars or the mouse wheel goes through here.
    BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE) override
    {
        const auto result = DoubleBufferViewBase::OnScrollBy(sizeScroll, bDoScroll);
        // Rebuild the cache and repaint everything only when scrolling actually happened.
        if (result && bDoScroll) {
            Update();
            Invalidate(FALSE);
        }
        return result;
    }

    // The cache is built relative to the current scroll position.
    [[nodiscard]] CPoint GetCacheOrigin() const override
    { return GetDeviceScrollPosition(); }

    DECLARE_DYNCREATE(DoubleBufferScrollView)
    DECLARE_MESSAGE_MAP()
};

} // namespace Shos::MFCDoubleBuffer
