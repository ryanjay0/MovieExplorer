#include "stdafx.h"
#include "MovieExplorer.h"

static bool IsDigit(TCHAR ch) { return ch >= _T('0') && ch <= _T('9'); }

void ParseFileName(RString_ strFileName, RString_ strFullPath, RString &strTitle, RString &strYear, INT_PTR &nSeason, INT_PTR &nEpisode, RString &strAirDate, BYTE &bType)
{
	INT_PTR m, n;
	RString strTemp, strSeason, strEpisode;
	RString strYearTmp, strMonthTmp, strDayTmp;

	strTitle = strFileName;
	strYear.Empty();

	// only keep file name

	n = strTitle.ReverseFind(_T('\\'));
	if (n != -1)
		strTitle = strTitle.Mid(n + 1);
	//strTitle.MakeLower();

	// strip file extension (max 4 chars)

	n = strTitle.ReverseFind(_T('.'));
	if (n != -1 && strTitle.GetLength() - n < 6)
		strTitle = strTitle.Left(n);

	// Fix 5: strip leading track numbers (1-2 digits + space + uppercase letter)

	if (strTitle.GetLength() >= 4)
	{
		INT_PTR nDigits = 0;
		while (nDigits < strTitle.GetLength() && IsDigit(strTitle[nDigits]))
			nDigits++;
		if (nDigits >= 1 && nDigits <= 2 && nDigits < strTitle.GetLength() && strTitle[nDigits] == _T(' '))
		{
			INT_PTR nAfterSpace = nDigits + 1;
			if (nAfterSpace < strTitle.GetLength() && strTitle[nAfterSpace] >= _T('A') && strTitle[nAfterSpace] <= _T('Z'))
				strTitle = strTitle.Mid(nAfterSpace);
		}
	}

	// replace []{} by ()

	strTitle.Replace(_T('['), _T('('));
	strTitle.Replace(_T(']'), _T(')'));
	strTitle.Replace(_T('{'), _T('('));
	strTitle.Replace(_T('}'), _T(')'));

	/*
	// replace anything not ,()0-9a-zA-Z by a space

	for (int i = 0; i < strTitle.GetLength(); i++)
	if (!(strTitle[i] == _T(',')) &&
	!(strTitle[i] == _T('\'')) &&
	!(strTitle[i] == _T('(')) &&
	!(strTitle[i] == _T(')')) &&
	!(strTitle[i] >= _T('0') && strTitle[i] <= _T('9')) &&
	!(strTitle[i] >= _T('A') && strTitle[i] <= _T('Z')) &&
	!(strTitle[i] >= _T('a') && strTitle[i] <= _T('z')))
	*((LPTSTR)(LPCTSTR)strTitle + i) = _T(' ');
	*/

	// remove urls

	RString strUrl;
	if (GetFirstMatch(strTitle, _T("([wW][wW][wW]\\.[^\\.]*?\\.[cC][oO][mM])"), &strUrl, NULL))
	{
		m = strTitle.Find(strUrl, 0);
		if (m >= 0)
			strTitle = strTitle.Left(m) + strTitle.Right(strTitle.GetLength() - (m + strUrl.GetLength()));
	}

	// Fix 4: smart dot replacement — preserve dots between non-year digits

	{
		RString strResult;
		for (INT_PTR i = 0; i < strTitle.GetLength(); ++i)
		{
			if (strTitle[i] == _T('.'))
			{
				bool bDigitBefore = (i > 0 && IsDigit(strTitle[i - 1]));
				bool bDigitAfter = (i + 1 < strTitle.GetLength() && IsDigit(strTitle[i + 1]));

				if (bDigitBefore && bDigitAfter)
				{
					bool bPartOfYear = false;
					if (i >= 3)
					{
						bool bBeforeYear = (i == 3 || !IsDigit(strTitle[i - 4]));
						if (bBeforeYear)
						{
							RString strPossYear = strTitle.Mid(i - 3, 4);
							INT_PTR nYear = StringToNumber(strPossYear);
							if (nYear > 1900 && nYear < 2100)
								bPartOfYear = true;
						}
					}

					if (bPartOfYear)
					{
						TCHAR sz[2] = { _T(' '), 0 };
						strResult += sz;
					}
					else
					{
						INT_PTR nBeforeLen = 0;
						for (INT_PTR j = i - 1; j >= 0 && IsDigit(strTitle[j]); --j)
							nBeforeLen++;

						INT_PTR nAfterLen = 0;
						for (INT_PTR j = i + 1; j < strTitle.GetLength() && IsDigit(strTitle[j]); ++j)
							nAfterLen++;

						if (nBeforeLen <= 2 && nAfterLen <= 2)
						{
							TCHAR sz[2] = { _T(':'), 0 };
							strResult += sz;
						}
						else
						{
							TCHAR sz[2] = { _T(' '), 0 };
							strResult += sz;
						}
					}
				}
				else
				{
					TCHAR sz[2] = { _T(' '), 0 };
					strResult += sz;
				}
			}
			else
			{
				TCHAR sz[2] = { strTitle[i], 0 };
				strResult += sz;
			}
		}
		strTitle = strResult;
	}

	// replace _ by a space

	strTitle.Replace(_T('_'), _T(' '));

	// smart hyphen replacement — only replace separator hyphens

	{
		RString strResult;
		for (INT_PTR i = 0; i < strTitle.GetLength(); ++i)
		{
			if (strTitle[i] == _T('-'))
			{
				bool bLeftSpace = (i == 0 || strTitle[i - 1] == _T(' '));
				bool bRightSpace = (i + 1 >= strTitle.GetLength() || strTitle[i + 1] == _T(' '));
				if (bLeftSpace || bRightSpace)
					strResult += _T(" ");
				else
					strResult += _T("-");
			}
			else
			{
				TCHAR sz[2] = { strTitle[i], 0 };
				strResult += sz;
			}
		}
		strTitle = strResult;
	}

	// remove redundant space

	while (strTitle.Replace(_T("  "), _T(" ")));
	strTitle.Trim();

	// Fix 3: strip text after " - " when right side starts with a genre keyword

	{
		INT_PTR nDashPos = strTitle.Find(_T(" - "));
		if (nDashPos > 0)
		{
			RString strRight = strTitle.Mid(nDashPos + 3);
			strRight.Trim();
			RString strFirstWord;
			INT_PTR nSpace = strRight.Find(_T(' '));
			if (nSpace > 0)
				strFirstWord = strRight.Left(nSpace);
			else
				strFirstWord = strRight;
			strFirstWord.MakeLower();

			static const RString strGenres[] = {
				_T("horror"), _T("thriller"), _T("sci-fi"), _T("scifi"),
				_T("comedy"), _T("drama"), _T("action"), _T("romance"),
				_T("musical"), _T("documentary"), _T("fantasy"), _T("western"),
				_T("mystery"), _T("crime"), _T("animation"), _T("war"),
				_T("biography"), _T("film-noir"), _T("erotic"), _T("exploitation"),
				_T("sexploitation")
			};

			for (INT_PTR gi = 0; gi < sizeof(strGenres) / sizeof(strGenres[0]); ++gi)
			{
				if (strFirstWord == strGenres[gi])
				{
					strTitle = strTitle.Left(nDashPos);
					break;
				}
			}
		}
	}

	while (strTitle.Replace(_T("  "), _T(" ")));
	strTitle.Trim();

	// find year between (), strip anything following it

	for (n = 0; (n = strTitle.Find(_T('('), n)) != -1; n++)
	{
		if (n + 5 < strTitle.GetLength() && strTitle[n + 5] == _T(')'))
		{
			strTemp = strTitle.Mid(n + 1, 4);
			if (StringToNumber(strTemp) > 1900)
			{
				strYear = strTemp;
				strTitle = strTitle.Left(n);
				break;
			}
		}
	}

	// remove anything between ()

	for (m = 0, n = 0; (m = strTitle.Find(_T('('), n)) != -1 &&
		(n = strTitle.Find(_T(')'), m)) != -1; m = 0, n = 0)
		strTitle = strTitle.Left(m) + strTitle.Mid(n + 1);

	// for TV shows. Find the season and episode and remove everything following it

	if (GetFirstMatch(strTitle, _T("([Ss]\\d?\\d[Ee]\\d?\\d)"), &strTemp, NULL))
	{
		m = strTitle.Find(strTemp, 0);
		if (GetFirstMatch(strTemp, _T("[Ss](\\d?\\d)[Ee](\\d?\\d)"), &strSeason, &strEpisode, NULL))
		{
			nSeason = StringToNumber(strSeason);
			nEpisode = StringToNumber(strEpisode);
			bType = DB_TYPE_TV;
		}

		if (m >= 0)
			strTitle = strTitle.Left(m);
	}

	// for TV shows by date and parse date aired

	if (GetFirstMatch(strTitle, _T("(\\d\\d\\d\\d) (\\d?\\d) (\\d?\\d)"), &strYearTmp, &strMonthTmp, &strDayTmp, NULL))
	{
		const RString Month[12] = { _T("Jan"), _T("Feb"), _T("Mar"), _T("Apr"),
			_T("May"), _T("Jun"), _T("Jul"), _T("Aug"), _T("Sep"), _T("Oct"), _T("Nov"), _T("Dec") };

		strAirDate = strDayTmp + _T(" ") + Month[StringToNumber(strMonthTmp) - 1] + _T(". ") + strYearTmp;
		bType = DB_TYPE_TV;
	}

	// find year not in (), but not as first word, strip anything following it

	strTitle.Replace(_T('('), _T(' '));
	strTitle.Replace(_T(')'), _T(' '));
	while (strTitle.Replace(_T("  "), _T(" ")));
	strTitle.Trim();

	if (strYear.IsEmpty())
	{
		m = 0;
		while (m < strTitle.GetLength())
		{
			n = strTitle.Find(_T(' '), m);

			if (n == -1)
				n = strTitle.GetLength();

			if (m == n)
			{
				m = n + 1; continue;
			}

			if (n - m == 4)
			{
				strTemp = strTitle.Mid(m, n - m);
				if (StringToNumber(strTemp) > 1900 && m > 0)
				{
					strYear = strTemp;
					strTitle = strTitle.Left(m);
					break;
				}
			}

			m = n + 1;
		}
	}

	// strip 'season[s] \\d' and anything following it
	RString strSeasons;
	if (GetFirstMatch(strTitle, _T("([Ss]eason[s]? ?\\d?\\d(?: ?- ?\\d?\\d)?)"), &strSeasons, NULL))
	{
		m = strTitle.Find(strSeasons, 0);
		if (m >= 0)
			strTitle = strTitle.Left(m);
		bType = DB_TYPE_TV;
	}

	//Remove episode numbers of the form XXofYY - TODO: process to return episode number

	RString strOf;
	if (GetFirstMatch(strTitle, _T("(\\d?\\d[oO][fF]\\d?\\d)"), &strOf, NULL))
	{
		m = strTitle.Find(strOf, 0);
		if (m > 0)
			strTitle = strTitle.Left(m);
		bType = DB_TYPE_TV;
	}

	// strip common movie descriptors and everything after

	static const RString strDescriptors[] = { _T("webrip"), _T("dvdrip"), _T("dvdscr"), _T("xvid"), _T("bdrip"),
		_T("brrip"), _T("hdtv"), _T("pdtv"), _T("box set"), _T("box-set"), _T("x264"), _T("x265"),
		_T("1080p"), _T("720p"), _T("480p"), _T("2160p"), _T("4k"),
		_T("hevc"), _T("h264"), _T("h265"), _T("10bit"), _T("8bit"),
		_T("web dl"), _T("web dl"), _T("webrip"), _T("bluray"), _T("blu ray"),
		_T("amzn"), _T("nf"), _T("dsnp"), _T("hmax"), _T("dsnp"), _T("pmtp"),
		_T("ddp"), _T("dd"), _T("atmos"), _T("aac"), _T("flac"),
		_T("aac2"), _T("aac5"), _T("dd5"), _T("ddp5"), _T("ddp7"),
		_T("h265"), _T("hez8"), _T("bone"), _T("eztvx"), _T("playweb"),
		_T("remastered"), _T("extended"), _T("uncut"), _T("unrated"),
		_T("theatrical"), _T("uncorked"), _T("collector"), _T("special"),
		_T("shout"), _T("rarbg"), _T("tgx"), _T("yts"), _T("publichd") };
	foreach(strDescriptors, strD)
	{
		m = strTitle.FindNoCase(strD, 0);
		if (m > 0)
			strTitle = strTitle.Left(m);
	}

	// strip common tv network prefixes

	static const RString strNetworks[] = { _T("Discovery Channel"), _T("Discovery Ch"), _T("National Geographic"), _T("NG"), 
		_T("Ch4"), _T("PBS") };
	foreach(strNetworks, strN)
	{
		m = strTitle.Find(strN, 0);
		if (m == 0)
		{
			strTitle = strTitle.Right(strTitle.GetLength() - strN.GetLength());
			bType = DB_TYPE_TV;
		}
	}

	while (strTitle.Replace(_T("  "), _T(" ")));
	strTitle.Trim();

	// place some literals in front

	if (strTitle.GetLength() > 5 && strTitle.Right(5) == _T(", the"))
		strTitle = _T("the ") + strTitle.Left(strTitle.GetLength() - 5);
	if (strTitle.GetLength() > 5 && strTitle.Right(5) == _T(", The"))
		strTitle = _T("The ") + strTitle.Left(strTitle.GetLength() - 5);
	if (strTitle.GetLength() > 3 && strTitle.Right(3) == _T(", a"))
		strTitle = _T("a ") + strTitle.Left(strTitle.GetLength() - 3);
	if (strTitle.GetLength() > 3 && strTitle.Right(3) == _T(", A"))
		strTitle = _T("A ") + strTitle.Left(strTitle.GetLength() - 3);
	
	strTitle.Replace(_T(','), _T(' '));
	while (strTitle.Replace(_T("  "), _T(" ")));
	strTitle.Trim();

	// Fix 2: parent directory fallback — if parsed title is very short, try directory name

	if (strTitle.GetLength() < 3 && !strFullPath.IsEmpty())
	{
		RString strDirName = strFullPath;
		INT_PTR nLastSlash = strDirName.ReverseFind(_T('\\'));
		if (nLastSlash > 0)
		{
			RString strParent = strDirName.Left(nLastSlash);
			INT_PTR nPrevSlash = strParent.ReverseFind(_T('\\'));
			if (nPrevSlash >= 0)
				strDirName = strParent.Mid(nPrevSlash + 1);
			else
				strDirName.Empty();

			if (!strDirName.IsEmpty())
			{
				RString strAltTitle, strAltYear;
				INT_PTR nAltSeason = -1, nAltEpisode = -1;
				RString strAltAirDate;
				BYTE bAltType = DB_TYPE_UNKNOWN;
				ParseFileName(strDirName, RString(), strAltTitle, strAltYear, nAltSeason, nAltEpisode, strAltAirDate, bAltType);
				if (strAltTitle.GetLength() >= 3)
				{
					strTitle = strAltTitle;
					if (strYear.IsEmpty() && !strAltYear.IsEmpty())
						strYear = strAltYear;
					if (nSeason < 0 && nAltSeason >= 0)
						nSeason = nAltSeason;
					if (nEpisode < 0 && nAltEpisode >= 0)
						nEpisode = nAltEpisode;
					if (bType == DB_TYPE_UNKNOWN && bAltType != DB_TYPE_UNKNOWN)
						bType = bAltType;
				}
			}
		}
	}

	//TRACE0(_T("ParseFileName\n  strFileName = ") + strFileName + _T("\n  strTitle = ") +
	//		strTitle + _T("\n  strYear = ") + strYear + _T("\n"));
}
