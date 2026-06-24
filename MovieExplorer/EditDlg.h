#pragma once	

#define BUTTON_ID_REFRESH 7

class CEditDlg : public RDialog
{
	friend class RWindow;
	friend class RDialog;

public:
	CEditDlg(HWND hWndParent, DBMOVIE *pMov);
	~CEditDlg();

protected:
	void OnCommand(WORD id, WORD notifyCode, HWND hWndControl);
	bool OnCreate(CREATESTRUCT *pCS);
	void OnSize(DWORD type, WORD cx, WORD cy);
	void OnSizing(DWORD side, RECT *pRect);
	void OnOK();
	void OnRefresh();

	RButton m_btnOK, m_btnCancel, m_btnRefresh;
	RStatic m_stcFileName, m_stcFileName2, m_stcFileSize, m_stcFileSize2, m_stcTitle, m_stcTitle2, m_stcIMDb;
	REdit m_eIMDb;
	DBMOVIE *m_pMov;
};
