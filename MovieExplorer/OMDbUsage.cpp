#include "stdafx.h"
#include "MovieExplorer.h"
#include "OMDbUsage.h"

OMDbUsageTracker::OMDbUsageTracker()
{
	m_nCount = 0;
	m_nLimit = 900;
}

RString OMDbUsageTracker::TodayString()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	RString strYear = NumberToString((INT_PTR)st.wYear);
	RString strMonth = NumberToString((INT_PTR)st.wMonth);
	RString strDay = NumberToString((INT_PTR)st.wDay);

	if (strMonth.GetLength() == 1) strMonth = _T("0") + strMonth;
	if (strDay.GetLength() == 1) strDay = _T("0") + strDay;

	return strYear + _T("-") + strMonth + _T("-") + strDay;
}

void OMDbUsageTracker::CheckDateReset()
{
	RString strToday = TodayString();
	if (m_strDate != strToday)
	{
		m_strDate = strToday;
		m_nCount = 0;
	}
}

void OMDbUsageTracker::Load(RString strCacheDir)
{
	RLock lock(&m_cs);

	m_nLimit = GETPREFINT(_T("OMDbDailyLimit"));
	if (m_nLimit <= 0)
		m_nLimit = 900;

	m_strFilePath = CorrectPath(strCacheDir) + _T("\\omdb_usage.txt");

	m_strDate.Empty();
	m_nCount = 0;

	RString strContent;
	if (FileToString(m_strFilePath, strContent))
	{
		INT_PTR nNewline = strContent.Find(_T('\n'));
		if (nNewline > 0)
		{
			m_strDate = strContent.Left(nNewline);
			RString strCount = strContent.Mid(nNewline + 1);
			strCount.Trim(_T("\r\n"));
			m_nCount = StringToNumber(strCount);
		}
	}

	CheckDateReset();
}

void OMDbUsageTracker::Save()
{
	if (m_strFilePath.IsEmpty())
		return;
	INT_PTR nSlash = m_strFilePath.ReverseFind(_T('\\'));
	if (nSlash > 0)
	{
		RString strDir = m_strFilePath.Left(nSlash);
		if (!DirectoryExists(strDir))
			CreateDirectory(strDir);
	}
	RString strContent = m_strDate + _T("\n") + NumberToString(m_nCount) + _T("\n");
	StringToFile(strContent, m_strFilePath);
}

bool OMDbUsageTracker::RequestAllowed()
{
	RLock lock(&m_cs);
	CheckDateReset();
	return m_nCount < m_nLimit;
}

void OMDbUsageTracker::Increment()
{
	RLock lock(&m_cs);
	CheckDateReset();
	m_nCount++;
	Save();
}

INT_PTR OMDbUsageTracker::GetCount()
{
	RLock lock(&m_cs);
	CheckDateReset();
	return m_nCount;
}

INT_PTR OMDbUsageTracker::GetLimit()
{
	RLock lock(&m_cs);
	return m_nLimit;
}

void OMDbUsageTracker::Reset()
{
	RLock lock(&m_cs);
	m_nCount = 0;
	Save();
}
