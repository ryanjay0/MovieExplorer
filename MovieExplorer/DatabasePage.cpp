#include "stdafx.h"
#include "MovieExplorer.h"
#include "DatabasePage.h"

CDatabasePage::CDatabasePage()
{
}

CDatabasePage::~CDatabasePage()
{
}

void CDatabasePage::ApplyChanges()
{
	bool bRequireSync = false;
	bool bRequireUpdate = false;

	// IndexExtensions

	RString strIndexExtensions = m_eIndexExtensions.GetText();
	strIndexExtensions.Replace(_T(" "), _T(""));
	strIndexExtensions.Replace(_T(';'), _T('|'));
	strIndexExtensions.Trim(_T(";"));
	strIndexExtensions.MakeLower();

	if (strIndexExtensions != GETPREFSTR(_T("Database"), _T("IndexExtensions")))
	{
		SETPREFSTR(_T("Database"), _T("IndexExtensions"), strIndexExtensions);
		bRequireSync = true;
	}

	// IndexDirectories

	bool bIndexDirs = m_chkIndexDirectories.GetCheck();
	if (bIndexDirs != GETPREFBOOL(_T("Database"), _T("IndexDirectories")))
	{
		SETPREFBOOL(_T("Database"), _T("IndexDirectories"), bIndexDirs);
		bRequireSync = true;
	}

	// MaxInfoAge

	INT_PTR nMaxInfoAge = StringToNumber(m_eMaxInfoAge.GetText());
	if (nMaxInfoAge <= 0)
	{
		nMaxInfoAge = 2;
		m_eMaxInfoAge.SetText(_T("2"));
	}

	if (nMaxInfoAge != GETPREFINT(_T("Database"), _T("MaxInfoAge")))
	{
		SETPREFINT(_T("Database"), _T("MaxInfoAge"), (int)nMaxInfoAge);
		bRequireUpdate = true;
	}

	// OMDbAPIKey

	RString strOMDbAPIKey = m_eOMDbAPIKey.GetText();
	strOMDbAPIKey.Trim();
	if (strOMDbAPIKey != GETPREFSTR(_T("OMDbAPIKey")))
	{
		SETPREFSTR(_T("OMDbAPIKey"), strOMDbAPIKey);
		bRequireUpdate = true;
	}

	// TMDBAPIKey

	RString strTMDBAPIKey = m_eTMDBAPIKey.GetText();
	strTMDBAPIKey.Trim();
	if (strTMDBAPIKey != GETPREFSTR(_T("TMDBAPIKey")))
	{
		SETPREFSTR(_T("TMDBAPIKey"), strTMDBAPIKey);
		bRequireUpdate = true;
	}

	// OMDbDailyLimit

	INT_PTR nDailyLimit = StringToNumber(m_eDailyLimit.GetText());
	if (nDailyLimit < 1)
	{
		nDailyLimit = 900;
		m_eDailyLimit.SetText(_T("900"));
	}

	if (nDailyLimit != GETPREFINT(_T("OMDbDailyLimit")))
	{
		SETPREFINT(_T("OMDbDailyLimit"), (int)nDailyLimit);
	}

	// Services

	RString str = m_cbOnlyUse.GetText(m_cbOnlyUse.GetSel());
	if (m_cbOnlyUse.GetSel() == m_cbOnlyUse.GetCount()-1)
		str.Empty();
	if (str != GETPREFSTR(_T("InfoService"), _T("OnlyUse")))
		{SETPREFSTR(_T("InfoService"), _T("OnlyUse"), str); bRequireUpdate = true;}

	// Sync and update

	if (bRequireUpdate)
	{
		foreach (GetDB()->m_categories, cat)
			foreach (cat.directories, dir)
				foreach (dir.movies, mov)
					mov.bUpdated = false;
	}

	if (bRequireSync)
		GetDB()->SyncAndUpdate();
	else if (bRequireUpdate)
		GetDB()->Update();

	RPropertyPage::ApplyChanges();
}

bool CDatabasePage::OnCreate(CREATESTRUCT *pCS)
{
	if (!RPropertyPage::OnCreate(pCS))
		ASSERTRETURN(false);

	// Create controls

	m_bInitialized = false;

	DWORD cbStyle = CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP;

	if (
			!m_eIndexExtensions.Create<REdit>(m_hWnd, ES_AUTOHSCROLL|WS_TABSTOP, WS_EX_CLIENTEDGE) ||
			!m_stcIndexExtensions.Create<RStatic>(m_hWnd) ||
			!m_chkIndexDirectories.Create<RButton>(m_hWnd, BS_AUTOCHECKBOX|WS_TABSTOP) ||
			!m_eMaxInfoAge.Create<REdit>(m_hWnd, ES_AUTOHSCROLL|WS_TABSTOP|ES_NUMBER, WS_EX_CLIENTEDGE) ||
			!m_stcMaxInfoAge.Create<RStatic>(m_hWnd) ||
			!m_eOMDbAPIKey.Create<REdit>(m_hWnd, ES_AUTOHSCROLL|WS_TABSTOP, WS_EX_CLIENTEDGE) ||
			!m_stcOMDbAPIKey.Create<RStatic>(m_hWnd) ||
			!m_eTMDBAPIKey.Create<REdit>(m_hWnd, ES_AUTOHSCROLL|WS_TABSTOP, WS_EX_CLIENTEDGE) ||
			!m_stcTMDBAPIKey.Create<RStatic>(m_hWnd) ||
			!m_btnRecheckFailed.Create<RButton>(m_hWnd, WS_TABSTOP) ||
			!m_stcDailyLimit.Create<RStatic>(m_hWnd) ||
			!m_eDailyLimit.Create<REdit>(m_hWnd, ES_AUTOHSCROLL|WS_TABSTOP|ES_NUMBER, WS_EX_CLIENTEDGE) ||
			!m_stcUsageToday.Create<RStatic>(m_hWnd) ||
			!m_stcAttribution.Create<RStatic>(m_hWnd) ||
			!m_grpDatabase.Create<RButton>(m_hWnd, BS_GROUPBOX) ||

		!m_cbOnlyUse.Create<RComboBox>(m_hWnd, cbStyle) ||

		!m_stcOnlyUse.Create<RStatic>(m_hWnd) ||
		!m_grpInfoService.Create<RButton>(m_hWnd, BS_GROUPBOX)
		)
		ASSERTRETURN(false);

	// Initialize controls

	RString strIndexExtensions = GETPREFSTR(_T("Database"), _T("IndexExtensions"));
	strIndexExtensions.Replace(_T('|'), _T(';'));
	m_eIndexExtensions.SetText(strIndexExtensions);

	m_chkIndexDirectories.SetCheck(GETPREFBOOL(_T("Database"), _T("IndexDirectories")));

	m_eMaxInfoAge.SetText(GETPREFSTR(_T("Database"), _T("MaxInfoAge")));

	m_eOMDbAPIKey.SetText(GETPREFSTR(_T("OMDbAPIKey")));

	m_eTMDBAPIKey.SetText(GETPREFSTR(_T("TMDBAPIKey")));

	m_eDailyLimit.SetText(NumberToString(GETPREFINT(_T("OMDbDailyLimit"))));

	m_stcUsageToday.SetText(_T("Used: ") + NumberToString(GetDB()->m_usageTracker.GetCount()) +
		_T("/") + NumberToString(GetDB()->m_usageTracker.GetLimit()) + _T(" today"));


	// Populate services combo box

	RObArray<RString> services;
	services.Add(_T("tmdb.org"));
	services.Add(_T("imdb.com"));
	services.Add(_T(""));

	foreach (services, strServiceName)
		m_cbOnlyUse.Add(strServiceName);

	// Select service according to preferences

	m_cbOnlyUse.SetSel(GETPREFSTR(_T("InfoService"), _T("OnlyUse")));

	bool bCombined = (m_cbOnlyUse.GetSel() == m_cbOnlyUse.GetCount()-1);

	EnableWindow(m_cbOnlyUse, true);

	// Load control text

	OnPrefChanged();

	m_bInitialized = true;

	return true;
}

void CDatabasePage::OnSize(DWORD type, WORD cx, WORD cy)
{
	UNREFERENCED_PARAMETER(type);
	UNREFERENCED_PARAMETER(cy);
	int y = DUY(4);
	MoveWindow(m_grpDatabase, DUX(4), y, cx - DUX(8), DUY(136));
	y += DUY(12);
	MoveWindow(m_stcIndexExtensions, DUX(14), y, DUX(200), DUY(10));
	y += DUY(11);
	MoveWindow(m_eIndexExtensions, DUX(14), y, cx - DUX(28), DUY(12));
	y += DUY(15);
	MoveCheckBox(m_chkIndexDirectories, DUX(14), y);
	y += DUY(16);
	MoveStatic(m_stcMaxInfoAge, DUX(14), y+DUY(2));
	MoveWindow(m_eMaxInfoAge, DUX(14) + m_stcMaxInfoAge.GetWidth() + DUX(4), y, DUX(30), DUY(12));
	y += DUY(16);
	MoveStatic(m_stcOMDbAPIKey, DUX(14), y+DUY(2));
	MoveWindow(m_eOMDbAPIKey, DUX(14) + m_stcOMDbAPIKey.GetWidth() + DUX(4), y, cx - DUX(28) - m_stcOMDbAPIKey.GetWidth() - DUX(4), DUY(12));
	y += DUY(16);
	MoveStatic(m_stcTMDBAPIKey, DUX(14), y+DUY(2));
	MoveWindow(m_eTMDBAPIKey, DUX(14) + m_stcTMDBAPIKey.GetWidth() + DUX(4), y, cx - DUX(28) - m_stcTMDBAPIKey.GetWidth() - DUX(4), DUY(12));
	y += DUY(16);
	MoveWindow(m_btnRecheckFailed, DUX(14), y, DUX(80), DUY(12));
	y += DUY(16);
	MoveStatic(m_stcDailyLimit, DUX(14), y+DUY(2));
	MoveWindow(m_eDailyLimit, DUX(14) + m_stcDailyLimit.GetWidth() + DUX(4), y, DUX(40), DUY(12));
	MoveStatic(m_stcUsageToday, DUX(14) + m_stcDailyLimit.GetWidth() + DUX(4) + DUX(44), y+DUY(2));

	y = DUY(146);
	MoveWindow(m_grpInfoService, DUX(4), y, cx - DUX(8), DUY(30));
	y += DUY(12);
	MoveWindow(m_stcOnlyUse, DUX(14), y+DUY(1)+1, DUX(60), DUY(10));
	MoveWindow(m_cbOnlyUse, DUX(74), y, DUX(60), DUY(12));

	y = DUY(186);
	MoveStatic(m_stcAttribution, DUX(14), y+DUY(2));

	PostMessage(m_hWnd, WM_PAINT);
	PostChildren(m_hWnd, WM_PAINT);
}

void CDatabasePage::OnPrefChanged()
{
	RPropertyPage::OnPrefChanged();

	m_stcIndexExtensions.SetText(GETSTR(IDS_INDEXEXTENSIONS) + _T(":"));
	m_chkIndexDirectories.SetText(_T(" ") + GETSTR(IDS_INDEXDIRECTORIES));
	m_stcMaxInfoAge.SetText(GETSTR(IDS_MAXINFOAGE) + _T(":"));
	m_stcOMDbAPIKey.SetText(_T("OMDb API Key:"));
	m_stcTMDBAPIKey.SetText(_T("TMDB API Key:"));
	m_btnRecheckFailed.SetText(_T("Recheck Failed"));
	m_stcDailyLimit.SetText(_T("Daily API Limit:"));
	m_stcUsageToday.SetText(_T("Used: ") + NumberToString(GetDB()->m_usageTracker.GetCount()) +
		_T("/") + NumberToString(GetDB()->m_usageTracker.GetLimit()) + _T(" today"));
	m_grpDatabase.SetText(GETSTR(IDS_DATABASE));

	m_stcOnlyUse.SetText(GETSTR(IDS_ONLYUSE) + _T(":"));
	m_cbOnlyUse.SetText(m_cbOnlyUse.GetCount()-1, GETSTR(IDS_COMBINED));
	m_grpInfoService.SetText(GETSTR(IDS_INFOSERVICE));
	m_stcAttribution.SetText(_T("Data from TMDB / OMDb"));

	RECT rc;
	GetClientRect(m_hWnd, &rc);
	OnSize(0, (WORD)rc.right, (WORD)rc.bottom);
}

void CDatabasePage::OnDrawItem(UINT_PTR id, DRAWITEMSTRUCT *pDIS)
{
	RPropertyPage::OnDrawItem(id, pDIS);

	RString strText;
	INT_PTR nLen = SendMessage(pDIS->hwndItem, CB_GETLBTEXTLEN, (WPARAM)pDIS->itemID);
	if (nLen == CB_ERR)
		return;
	SendMessage(pDIS->hwndItem, CB_GETLBTEXT, (WPARAM)pDIS->itemID, (LPARAM)strText.GetBuffer(nLen));
	strText.ReleaseBuffer(nLen);

	SetBkColor(pDIS->hDC, (pDIS->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW)));
	ExtTextOut(pDIS->hDC, 0, 0, ETO_OPAQUE, &pDIS->rcItem, NULL, 0, NULL);
	SetTextColor(pDIS->hDC, (pDIS->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHTTEXT) : 
		(pDIS->itemState & ODS_DISABLED ? GetSysColor(COLOR_GRAYTEXT) : GetSysColor(COLOR_WINDOWTEXT))));
	TextOut(pDIS->hDC, DUX(1) + pDIS->rcItem.left, pDIS->rcItem.top, strText);
}

void CDatabasePage::OnMeasureItem(UINT_PTR id, MEASUREITEMSTRUCT *pMIS)
{
	RPropertyPage::OnMeasureItem(id, pMIS);
	pMIS->itemHeight = DUY(8);
}

void CDatabasePage::OnCommand(WORD id, WORD notifyCode, HWND hWndControl)
{
	RPropertyPage::OnCommand(notifyCode, id, hWndControl);

	if (!m_bInitialized)
		return;

	if (notifyCode == CBN_SELENDOK || notifyCode == BN_CLICKED || notifyCode == EN_CHANGE)
	{
		if (notifyCode == BN_CLICKED && hWndControl == m_btnRecheckFailed)
		{
			INT_PTR nCount = 0;
			foreach (GetDB()->m_categories, cat)
				foreach (cat.directories, dir)
					foreach (dir.movies, mov)
					{
						if (mov.strIMDbID == _T("unknown") || mov.strIMDbID == _T("connError") ||
							mov.strIMDbID == _T("scrapeError") || mov.strIMDbID == _T("rateLimited") ||
						mov.strTMDBID == _T("unknown") || mov.strTMDBID == _T("connError") ||
						mov.strTMDBID == _T("scrapeError") || mov.strTMDBID == _T("rateLimited"))
						{
							mov.strIMDbID.Empty();
							mov.strTMDBID.Empty();
							mov.bUpdated = false;
							nCount++;
						}
					}
			if (nCount > 0)
			{
				RString strMsg = NumberToString(nCount) + _T(" movie") + (nCount > 1 ? _T("s") : _T("")) + _T(" queued for re-check.");
				MessageBox(m_hWnd, strMsg, _T("Recheck Failed"), MB_OK|MB_ICONINFORMATION);
				GetDB()->Update();
			}
			else
				MessageBox(m_hWnd, _T("No failed movies to recheck."), _T("Recheck Failed"), MB_OK|MB_ICONINFORMATION);
			return;
		}

		SetChanged();

		if (hWndControl == m_cbOnlyUse)
		{
			bool bCombined = (m_cbOnlyUse.GetSel() == m_cbOnlyUse.GetCount()-1);

			EnableWindow(m_cbTitle, bCombined);
			EnableWindow(m_cbYear, bCombined);
			EnableWindow(m_cbGenres, bCombined);
			EnableWindow(m_cbCountries, bCombined);
			EnableWindow(m_cbRuntime, bCombined);
			EnableWindow(m_cbStoryline, bCombined);
			EnableWindow(m_cbDirectors, bCombined);
			EnableWindow(m_cbWriters, bCombined);
			EnableWindow(m_cbStars, bCombined);
			EnableWindow(m_cbPoster, bCombined);
			EnableWindow(m_cbRating, bCombined);

			if (!bCombined)
			{
				m_cbTitle.SetSel(m_cbTitle.GetCount()-1);
				m_cbYear.SetSel(m_cbYear.GetCount()-1);
				m_cbGenres.SetSel(m_cbGenres.GetCount()-1);
				m_cbCountries.SetSel(m_cbCountries.GetCount()-1);
				m_cbRuntime.SetSel(m_cbRuntime.GetCount()-1);
				m_cbStoryline.SetSel(m_cbStoryline.GetCount()-1);
				m_cbDirectors.SetSel(m_cbDirectors.GetCount()-1);
				m_cbWriters.SetSel(m_cbWriters.GetCount()-1);
				m_cbStars.SetSel(m_cbStars.GetCount()-1);
				m_cbPoster.SetSel(m_cbPoster.GetCount()-1);
				m_cbRating.SetSel(m_cbRating.GetCount()-1);

				RString strServiceName = m_cbOnlyUse.GetText(m_cbOnlyUse.GetSel());

				m_cbTitle.SetSel(strServiceName);
				m_cbYear.SetSel(strServiceName);
				m_cbGenres.SetSel(strServiceName);
				m_cbCountries.SetSel(strServiceName);
				m_cbRuntime.SetSel(strServiceName);
				m_cbStoryline.SetSel(strServiceName);
				m_cbDirectors.SetSel(strServiceName);
				m_cbWriters.SetSel(strServiceName);
				m_cbStars.SetSel(strServiceName);
				m_cbPoster.SetSel(strServiceName);
				m_cbRating.SetSel(strServiceName);
			}
		}
	}
}
