#pragma once

#include <map>

struct SeriesCache;
struct SeriesSeasonData;

DWORD ScrapeIMDb(DBINFO *pInfo, RString strOMDbAPIKey, std::map<RString, SeriesCache> *pSeriesCache = NULL);
