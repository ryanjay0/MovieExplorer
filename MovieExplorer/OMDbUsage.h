#pragma once

#include "..\RClasses\RCriticalSection.h"

class OMDbUsageTracker
{
public:
	OMDbUsageTracker();

	bool RequestAllowed();
	void Increment();
	INT_PTR GetCount();
	INT_PTR GetLimit();
	void Reset();
	void Load(RString strCacheDir);
	void Save();

private:
	RCriticalSection m_cs;
	INT_PTR m_nCount;
	INT_PTR m_nLimit;
	RString m_strDate;
	RString m_strFilePath;

	void CheckDateReset();
	RString TodayString();
};
