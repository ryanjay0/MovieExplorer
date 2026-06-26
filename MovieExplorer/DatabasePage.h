#pragma once

class CDatabasePage : public RPropertyPage
{
	friend class RWindow;

public:
	CDatabasePage();
	~CDatabasePage();
	virtual void ApplyChanges();

protected:
	bool OnCreate(CREATESTRUCT *pCS);
	void OnSize(DWORD type, WORD cx, WORD cy);
	void OnPrefChanged();
	void OnDrawItem(UINT_PTR id, DRAWITEMSTRUCT *pDIS);
	void OnMeasureItem(UINT_PTR id, MEASUREITEMSTRUCT *pMIS);
	void OnCommand(WORD id, WORD notifyCode, HWND hWndControl);

	RStatic m_stcIndexExtensions, m_stcMaxInfoAge, m_stcOnlyUse, m_stcOMDbAPIKey,
			m_stcTMDBAPIKey, m_stcDailyLimit, m_stcUsageToday, m_stcAttribution;
	RComboBox m_cbOnlyUse;
	REdit m_eIndexExtensions, m_eMaxInfoAge, m_eOMDbAPIKey, m_eTMDBAPIKey, m_eDailyLimit;
	RButton m_grpDatabase, m_grpInfoService, m_chkIndexDirectories, m_btnRecheckFailed;
	bool m_bInitialized;
};
