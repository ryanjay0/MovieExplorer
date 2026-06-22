#pragma once

#include <map>
#include "..\RClasses\RCriticalSection.h"

struct SeriesSeasonData
{
	RObArray<RString> episodeTitles;
	RObArray<RString> episodeIDs;
	RObArray<RString> episodeRatings;
	RObArray<RString> episodeVotes;
	RObArray<RString> episodeReleased;
};

struct SeriesCache
{
	std::map<int, SeriesSeasonData> seasons;
};

struct SeriesDedup
{
	RCriticalSection cs;
	std::map<RString, RString> searchToID;

	RString Lookup(RString strKey)
	{
		RLock lock(&cs);
		auto it = searchToID.find(strKey);
		if (it != searchToID.end())
			return it->second;
		return RString();
	}

	void Store(RString strKey, RString strID)
	{
		RLock lock(&cs);
		searchToID[strKey] = strID;
	}
};

struct OMDbUsageTracker;

struct UPDATETHREADDATA
{
	HWND hDatabaseWnd;
	REvent eReady;
	SeriesDedup *pDedup;
	OMDbUsageTracker *pUsage;
};

UINT CALLBACK UpdateThread(void *pParam);
