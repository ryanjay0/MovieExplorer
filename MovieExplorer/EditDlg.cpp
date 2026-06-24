#include "stdafx.h"
#include "MovieExplorer.h"
#include "EditDlg.h"

CEditDlg::CEditDlg(HWND hWndParent, DBMOVIE* pMov) : m_pMov(pMov)
{
	VERIFY(Create<CEditDlg>(hWndParent, GETSTR(IDS_EDITMOVIE), 0, 0, DUX(250), DUY(130), true));
	VERIFY(ShowModal<CEditDlg>(true));
}

CEditDlg::~CEditDlg()
{
}

void CEditDlg::OnCommand(WORD id, WORD notifyCode, HWND hWndControl)
{
	RDialog::OnCommand(id, notifyCode, hWndControl);

	switch (id)
	{
	case IDOK:
		OnOK();
		break;
	case IDCANCEL:
		PostMessage(m_hWnd, WM_CLOSE);
		break;
	case BUTTON_ID_REFRESH:
		OnRefresh();
		break;
	}
}

bool CEditDlg::OnCreate(CREATESTRUCT *pCS)
{
	if (!RDialog::OnCreate(pCS))
		return false;

	if (!m_btnOK.Create<RButton>(m_hWnd, WS_TABSTOP|BS_DEFPUSHBUTTON, 0, GETSTR(IDS_OK), 0, 0, 0, 0, IDOK) ||
			!m_btnCancel.Create<RButton>(m_hWnd, WS_TABSTOP, 0, GETSTR(IDS_CANCEL), 0, 0, 0, 0, IDCANCEL) ||
			!m_btnRefresh.Create<RButton>(m_hWnd, WS_TABSTOP, 0, _T("Refresh from Web"), 0, 0, 0, 0, BUTTON_ID_REFRESH) ||
			!m_stcFileName.Create<RStatic>(m_hWnd, 0, 0, GETSTR(IDS_FILENAME) + _T(":")) ||
			!m_stcFileName2.Create<RStatic>(m_hWnd, 0, 0, m_pMov->strFileName) ||
			!m_stcFileSize.Create<RStatic>(m_hWnd, 0, 0, GETSTR(IDS_FILESIZE) + _T(":")) ||
			!m_stcFileSize2.Create<RStatic>(m_hWnd, 0, 0, SizeToString(m_pMov->fileSize)) ||
			!m_stcTitle.Create<RStatic>(m_hWnd, 0, 0, _T("Title:")) ||
			!m_stcTitle2.Create<RStatic>(m_hWnd, 0, 0, m_pMov->strTitle) ||
			!m_stcIMDb.Create<RStatic>(m_hWnd, 0, 0, _T("IMDb ID:")) ||
			!m_eIMDb.Create<REdit>(m_hWnd, WS_TABSTOP|ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, m_pMov->strIMDbID))
		ASSERTRETURN(false);

	SetFocus(m_btnOK);
	return true;
}

void CEditDlg::OnSize(DWORD type, WORD cx, WORD cy)
{
	RDialog::OnSize(type, cx, cy);

	int y = DUY(6);
	MoveWindow(m_stcFileName, DUX(12), y+DUY(1)+1, DUX(60), DUY(10));
	MoveWindow(m_stcFileName2, DUX(12) + DUX(62), y+DUY(1)+1, cx - DUX(82), DUY(10));
	y += DUY(14);
	MoveWindow(m_stcFileSize, DUX(12), y+DUY(1)+1, DUX(60), DUY(10));
	MoveWindow(m_stcFileSize2, DUX(12) + DUX(62), y+DUY(1)+1, cx - DUX(82), DUY(10));
	y += DUY(14);
	MoveWindow(m_stcTitle, DUX(12), y+DUY(1)+1, DUX(60), DUY(10));
	MoveWindow(m_stcTitle2, DUX(12) + DUX(62), y+DUY(1)+1, cx - DUX(82), DUY(10));
	y += DUY(14);
	MoveWindow(m_stcIMDb, DUX(12), y+DUY(1)+1, DUX(60), DUY(10));
	MoveWindow(m_eIMDb, DUX(12) + DUX(62), y, cx - DUX(82), DUY(12));

	MoveWindow(m_btnCancel, cx - DUX(54), cy - DUY(20), DUX(50), DUY(14));
	MoveWindow(m_btnOK, cx - DUX(108), cy - DUY(20), DUX(50), DUY(14));
	MoveWindow(m_btnRefresh, DUX(12), cy - DUY(20), DUX(90), DUY(14));

	PostMessage(m_hWnd, WM_PAINT);
	PostChildren(m_hWnd, WM_PAINT);
}

void CEditDlg::OnSizing(DWORD side, RECT *pRect)
{
	pRect->bottom = pRect->top + DUY(132);
	RDialog::OnSizing(side, pRect);
}

void CEditDlg::OnOK()
{
	bool bIDChanged = (m_pMov->strIMDbID != m_eIMDb.GetText());

	if (bIDChanged)
	{
		GetDB()->CancelUpdate();

		m_pMov->fIMDbRating = m_pMov->fIMDbRatingMax = 0.0f;
		m_pMov->fRating = m_pMov->fRatingMax = 0.0f;
		m_pMov->nMetascore = -1;
		m_pMov->nEpisode = -1; m_pMov->nSeason = -1;
		m_pMov->strEpisodeName.Empty(); m_pMov->strEpisodeID.Empty(); m_pMov->strAirDate.Empty();
		m_pMov->bType = DB_TYPE_UNKNOWN;
		m_pMov->nIMDbVotes = m_pMov->nVotes = m_pMov->nRuntime = 0;
		m_pMov->strTitle.Empty(); m_pMov->strYear.Empty(); m_pMov->strCountries.Empty(); 
		m_pMov->strGenres.Empty(); m_pMov->strStoryline.Empty(); m_pMov->strContentRating.Empty();
		m_pMov->strDirectors.Empty(); m_pMov->strWriters.Empty(); m_pMov->strStars.Empty();
		m_pMov->posterData.SetSize(0); 
		for (int i = 0; i < DBI_STAR_NUMBER; i++)
		{
			m_pMov->strActorId[i].Empty();
			m_pMov->actorImageData[i] = NULL;
		}
	}

	m_pMov->strIMDbID = m_eIMDb.GetText();

	if (bIDChanged)
		m_pMov->bUpdated = false;

	GetDB()->Update(m_pMov);

	PostMessage(GetMainWnd(), WM_DBUPDATED);
	PostMessage(m_hWnd, WM_CLOSE);
}

void CEditDlg::OnRefresh()
{
	GetDB()->CancelUpdate();

	RString strCacheDir = CorrectPath(GETPREFSTR(_T("Database"), _T("CacheDirectory")));
	if (!m_pMov->strIMDbID.IsEmpty() && !strCacheDir.IsEmpty())
	{
		RString strID = m_pMov->strIMDbID;
		RString strBase = strCacheDir + _T("\\imdb.com\\") + strID;
		DeleteFile(strBase + _T(".xml"));
		if (m_pMov->nSeason >= 0 && m_pMov->nEpisode >= 0)
			DeleteFile(strBase + _T("_S") + NumberToString(m_pMov->nSeason) + _T("_E") + NumberToString(m_pMov->nEpisode) + _T(".xml"));
	}

	m_pMov->fIMDbRating = m_pMov->fIMDbRatingMax = 0.0f;
	m_pMov->fRating = m_pMov->fRatingMax = 0.0f;
	m_pMov->nMetascore = -1;
	m_pMov->nEpisode = -1; m_pMov->nSeason = -1;
	m_pMov->strEpisodeName.Empty(); m_pMov->strEpisodeID.Empty(); m_pMov->strAirDate.Empty();
	m_pMov->bType = DB_TYPE_UNKNOWN;
	m_pMov->nIMDbVotes = m_pMov->nVotes = m_pMov->nRuntime = 0;
	m_pMov->strTitle.Empty(); m_pMov->strYear.Empty(); m_pMov->strCountries.Empty(); 
	m_pMov->strGenres.Empty(); m_pMov->strStoryline.Empty(); m_pMov->strContentRating.Empty();
	m_pMov->strDirectors.Empty(); m_pMov->strWriters.Empty(); m_pMov->strStars.Empty();
	m_pMov->posterData.SetSize(0); 
	for (int i = 0; i < DBI_STAR_NUMBER; i++)
	{
		m_pMov->strActorId[i].Empty();
		m_pMov->actorImageData[i] = NULL;
	}

	m_pMov->bUpdated = false;

	GetDB()->Update(m_pMov);

	PostMessage(GetMainWnd(), WM_DBUPDATED);
	PostMessage(m_hWnd, WM_CLOSE);
}
