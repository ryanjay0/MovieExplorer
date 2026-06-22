#pragma once

#include <map>

struct SeriesCache;
struct SeriesSeasonData;
class OMDbUsageTracker;

DWORD ScrapeIMDb(DBINFO *pInfo, RString strOMDbAPIKey, std::map<RString, SeriesCache> *pSeriesCache = NULL, OMDbUsageTracker *pUsage = NULL);
