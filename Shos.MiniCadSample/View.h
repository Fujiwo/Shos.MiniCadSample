#pragma once
//#define SCROLL_VIEW

#include "DoubleBuffer.h"
#include "ClipboardHelper.h"

class View : public
#ifdef SCROLL_VIEW
	DoubleBufferScrollView
#else // SCROLL_VIEW
	DoubleBufferView
#endif // SCROLL_VIEW 
{
public:
	View()
	{
		SetBackgroundColor(GetBackgroundColor());
	}
	
protected:
	Document& GetDocument() const
	{
		ASSERT_VALID(m_pDocument);
		return reinterpret_cast<Document&>(*m_pDocument);
	}

#ifdef SCROLL_VIEW
	virtual void View::OnInitialUpdate() override
	{
		DoubleBufferScrollView::OnInitialUpdate();
		SetScrollSizes(MM_TEXT, GetDocument().GetSize());
	}
#endif // SCROLL_VIEW 

#ifndef SCROLL_VIEW
	virtual void OnPrepareDC(CDC* dc, CPrintInfo* pInfo = nullptr) override
	{
		DoubleBufferView::OnPrepareDC(dc, pInfo);

		dc->SetMapMode(MM_ISOTROPIC);

		auto documentArea = GetDocument().GetArea();
		dc->SetWindowOrg(documentArea.CenterPoint());
		dc->SetWindowExt(documentArea.Size());

		CRect clientRect;
		GetClientRect(&clientRect);
		dc->SetViewportOrg(clientRect.CenterPoint());
		dc->SetViewportExt(clientRect.Size());
	}
#endif // SCROLL_VIEW

	//virtual void OnDraw(CDC* pDC) override
	//{
	//	DrawFigures(*pDC, GetDocument());
	//}

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

		if (pHint == nullptr || static_cast<Hint*>(pHint)->type == Hint::Type::ViewOnly) {
			#ifdef SCROLL_VIEW
			DoubleBufferScrollView
			#else // SCROLL_VIEW
			DoubleBufferView
			#endif // SCROLL_VIEW 
								  ::OnUpdate(pSender, lHint, pHint);
			return;
		}

		CRect area;
		FigureHelper::GetArea(static_cast<Hint*>(pHint)->figures, area);
		
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

	afx_msg void OnLButtonUp(UINT nFlags, CPoint point)
	{
		GetDocument().OnClick(DPtoLP(point));
	}

	afx_msg void OnMouseMove(UINT nFlags, CPoint point)
	{
		GetDocument().OnMouseMove(DPtoLP(point));
		Invalidate();
	}

	afx_msg void OnDestroyClipboard()
	{
		ClipboardHelper::OnDestroyClipboard();
	}

private:
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
		CClientDC dc(this);
		OnPrepareDC(&dc);
		dc.DPtoLP(&point);
		return point;
	}

	CRect LPtoDP(CRect rect)
	{
		CClientDC dc(this);
		OnPrepareDC(&dc);
		dc.LPtoDP(rect);
		return rect;
	}

	DECLARE_DYNCREATE(View)
	DECLARE_MESSAGE_MAP()
};
