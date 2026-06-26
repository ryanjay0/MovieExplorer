#include "stdafx.h"
#include "MovieExplorer.h"

bool CorrectPreferences()
{
	RPreferencesMgr2 prefMgr;
	prefMgr.Open(CorrectPath(_T("Preferences.xml")));

	prefMgr.SetStr(_T("LanguageFile"), _T("Languages\\English.xml"), false);

	prefMgr.SetStr(_T("ThemeFile"), _T("Themes\\Dark.xml"), false);

	prefMgr.SetStr(_T("DatabaseFile"), _T("Database.xml"), false);
	
	// TODO: determine max window dimension using GetSystemMetrics or something else

	prefMgr.SetInt(_T("MainWnd"), _T("x"), 150, false);
	prefMgr.SetInt(_T("MainWnd"), _T("y"), 30, false);
	prefMgr.SetInt(_T("MainWnd"), _T("cx"), 950, false);
	prefMgr.SetInt(_T("MainWnd"), _T("cy"), 750, false);
	prefMgr.SetBool(_T("MainWnd"), _T("Maximized"), false, false);

	if (prefMgr.GetInt(_T("MainWnd"), _T("x")) + prefMgr.GetInt(_T("MainWnd"), _T("cx")) < 10)
	{
		prefMgr.SetInt(_T("MainWnd"), _T("x"), 0);
		prefMgr.SetInt(_T("MainWnd"), _T("cx"), 900);
	}
	if (prefMgr.GetInt(_T("MainWnd"), _T("y")) + prefMgr.GetInt(_T("MainWnd"), _T("cy")) < 10)
	{
		prefMgr.SetInt(_T("MainWnd"), _T("y"), 0);
		prefMgr.SetInt(_T("MainWnd"), _T("cy"), 600);
	}

	prefMgr.SetFloat(_T("MainWnd"), _T("Zoom"), GetScale(), false);
	SetScale(prefMgr.GetFloat(_T("MainWnd"), _T("Zoom")));

	prefMgr.SetBool(_T("MainWnd"), _T("ShowStatusBar"), true, false);
	prefMgr.SetBool(_T("MainWnd"), _T("ShowLog"), true, false);
	prefMgr.SetInt(_T("MainWnd"), _T("LogHeight"), 108, false);

	prefMgr.SetStr(_T("Database"), _T("IndexExtensions"), _T("asf|avi|mkv|mp4|mpeg|mpg|wmv"), false);
	prefMgr.SetBool(_T("Database"), _T("IndexDirectories"), true, false);
	prefMgr.SetInt(_T("Database"), _T("MaxInfoAge"), 2);
	prefMgr.SetStr(_T("Database"), _T("CacheDirectory"), _T("Cache"), false);

	prefMgr.SetStr(_T("InfoService"), _T("OnlyUse"), _T("tmdb.org"), false);

	prefMgr.SetBool(_T("Search"), _T("Instantly"), true, false);
	prefMgr.SetBool(_T("Search"), _T("Literally"), false, false);
	prefMgr.SetBool(_T("Search"), _T("Storyline"), false, false);

	prefMgr.SetBool(_T("NormalizeRatings"), false, false);
	prefMgr.SetBool(_T("ShowSeenMovies"), false, false);
	prefMgr.SetBool(_T("ViewType"), false, false);
	prefMgr.SetBool(_T("ShowHiddenMovies"), false, false);
	prefMgr.SetInt(_T("SortBy"), 7, false);
	prefMgr.SetStr(_T("OMDbAPIKey"), _T(""), false);
	prefMgr.SetStr(_T("TMDBAPIKey"), _T(""), false);
	prefMgr.SetInt(_T("OMDbDailyLimit"), 900, false);
	prefMgr.SetBool(_T("AutoCategories"), true, false);
	prefMgr.SetBool(_T("HideUserCategories"), false, false);

	prefMgr.SetInt(_T("TouchScrollElapse"), 10, false);
	prefMgr.SetFloat(_T("TouchScrollCoeff"), 0.95f, false);
	
	if (!prefMgr.SaveAs(CorrectPath(_T("Preferences.xml"))))
		ASSERTRETURN(false);

	return true;
}