#pragma once

#include "ScrollBar.h"
#include "ToolBarButton.h"
#include "Resume.h"

class CListView : public RWindow
{
	friend class RWindow;

public:
	CListView();
	~CListView();
	void GoToItem(int item);

protected:
	void OnCommand(WORD id, WORD notifyCode, HWND hWndControl);
	bool OnCreate(CREATESTRUCT *pCS);
	void OnLButtonDown(DWORD keys, short x, short y);
	void OnLButtonDblClk(DWORD keys, short x, short y);
	void OnMouseMove(DWORD keys, short x, short y);
	void OnMouseWheel(WORD keys, short delta, short x, short y);
	void OnPaint(HDC hDC);
	void OnPrefChanged();
	void OnRButtonDown(DWORD keys, short x, short y);
	void OnScaleChanged();
	bool OnSetCursor(HWND hWnd, WORD hitTest, WORD mouseMsg);
	void OnSize(DWORD type, WORD cx, WORD cy);
	void OnTouch(WORD nInputs, HTOUCHINPUT hTouchInput);
	void OnTimer(UINT_PTR nIDEvent);
	void OnVScroll(WORD scrollCode, WORD pos, HWND hWndScrollBar);
	void OnKeyDown(UINT, WORD, UINT);
	LRESULT WndProc(UINT Msg, WPARAM wParam, LPARAM lParam);

	void Draw();


	RMemoryDC m_mdc, m_mdcSmallStar, m_mdcSmallStarEmpty, m_mdcLargeStar, m_mdcLargeStarEmpty,
			m_mdcPlayBtn, m_mdcDirBtn, m_mdcEditBtn, m_mdcSeenBtn, m_mdcHideBtn, m_mdcDeleteBtn;
	COLORREF m_clrBackgr, m_clrBackgrAlt, m_clrShadow, m_clrTitle, m_clrText, m_clrLink;
	BYTE m_aRebarShadow, m_aPosterShadow, m_aItemShadow;
	RSprite m_sprShadow;
	CScrollBar m_sb;
	RFont m_fntTitle, m_fntText, m_fntTextBold;
	int m_nColumnWidth;
	struct LINK {RString strText, strURL; RRect rc; UINT_PTR state;};
	RObArray<LINK> m_links;
	CToolBarButton m_btnPlay, m_btnDir, m_btnSeen, m_btnEdit, m_btnHide, m_btnDelete;
	INT_PTR m_nHoverMov;
	bool m_bScrolling, m_bNormalizeRatings, m_bCaptureM;
	RString m_strRatingServ;
	RObArray<RString> m_servicesInUse;
	double m_dTouchScrollSpeed;
	int m_nTouchScrollElapse;
	double m_dTouchScrollCoeff;

	bool m_bHideUserCategories;
	bool m_bUseVlc;

	Resume resume;

	LINK* MakeLink(RString strText, RString strUrl, INT_PTR x, int cx, INT_PTR cy, int y, POINT pt);

	static const COLORREF m_clrGood = RGB(102, 204, 51);
	static const COLORREF m_clrNeutral = RGB(255, 204, 51);
	static const COLORREF m_clrBad = RGB(255, 0, 0);
};
