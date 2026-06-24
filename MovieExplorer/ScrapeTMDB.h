#pragma once

#include "Database.h"
#include "UpdateThread.h"
#include "OMDbUsage.h"

DWORD ScrapeTMDB(DBINFO *pInfo, RString strTMDBAPIKey, RString strOMDbAPIKey,
	std::map<RString, SeriesCache> *pSeriesCache = NULL,
	OMDbUsageTracker *pUsage = NULL);
