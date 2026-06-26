#include "stdafx.h"
#include "MovieExplorer.h"
#include "ScrapeTMDB.h"
#include "JsonParser.h"

static RCriticalSection g_tmdbCS;
static DWORD g_tmdbLastRequest = 0;

static void TMDBThrottle()
{
	RLock lock(&g_tmdbCS);
	DWORD now = GetTickCount();
	DWORD elapsed = now - g_tmdbLastRequest;
	if (elapsed < 30)
		Sleep(30 - elapsed);
	g_tmdbLastRequest = GetTickCount();
}

static bool TMDBRequest(RString strURL, JsonDoc &doc)
{
	TMDBThrottle();
	RString strResponse;
	if (!URLToString(strURL, strResponse))
		return false;
	if (strResponse.IsEmpty())
		return false;
	return doc.Parse(strResponse);
}

static RString TMDBImageURL(RString strPath, const TCHAR *size)
{
	if (strPath.IsEmpty()) return _T("");
	return _T("https://image.tmdb.org/t/p/") + RString(size) + strPath;
}

static int TMDBPickBestResult(JsonDoc &doc, RString strSearchTitle, RString strSearchYear, BYTE bType, bool bIsTV, RString &strBestID)
{
	int resultsIdx = doc.FindKey(doc.RootIdx(), L"results");
	if (resultsIdx < 0) return 0;

	int count = doc.NCount(resultsIdx);
	if (count == 0) return 0;

	strBestID.Empty();
	int nBestScore = -1;
	RString strLowerSearch = strSearchTitle;
	strLowerSearch.MakeLower();

	for (int i = 0; i < count; i++)
	{
		int itemIdx = doc.NAt(resultsIdx, i);
		if (itemIdx < 0) continue;

		std::wstring titleKey = bIsTV ? L"name" : L"title";
		std::wstring dateKey = bIsTV ? L"first_air_date" : L"release_date";

		RString strTitle = doc.NGetStr(itemIdx, titleKey).c_str();
		RString strDate = doc.NGetStr(itemIdx, dateKey).c_str();
		int nID = doc.NGetInt(itemIdx, L"id", 0);
		double dVote = doc.NGetDbl(itemIdx, L"vote_average", 0);
		double dPop = doc.NGetDbl(itemIdx, L"popularity", 0);

		RString strLowerTitle = strTitle;
		strLowerTitle.MakeLower();

		int nScore = 0;

		if (_tcsicmp(strTitle, strSearchTitle) == 0)
			nScore += 100;
		else if (strLowerTitle.Find(strLowerSearch) >= 0)
			nScore += 50;

		if (!strSearchYear.IsEmpty())
		{
			RString strYear;
			INT_PTR nDash = strDate.Find(_T('-'));
			if (nDash > 0)
				strYear = strDate.Left(nDash);
			else
				strYear = strDate;
			if (strYear == strSearchYear)
				nScore += 30;
		}

		if (bType == DB_TYPE_TV && bIsTV)
			nScore += 20;
		else if (bType == DB_TYPE_MOVIE && !bIsTV)
			nScore += 20;

		nScore += (int)(dVote > 0 ? dVote : 0);
		nScore += (int)(dPop > 0 ? dPop / 10 : 0);

		if (nScore > nBestScore)
		{
			nBestScore = nScore;
			strBestID = NumberToString(nID);
		}
	}

	return !strBestID.IsEmpty();
}

static int TMDBSearch(RString strAPIKey, RString strTitle, RString strYear, BYTE bType, RString &strBestID)
{
	RString strEncoded = URLEncode(strTitle);

	RString strURL;
	if (bType == DB_TYPE_TV)
	{
		strURL = _T("https://api.themoviedb.org/3/search/tv?query=") + strEncoded +
			_T("&api_key=") + strAPIKey;
		if (!strYear.IsEmpty())
			strURL += _T("&first_air_date_year=") + strYear;
	}
	else
	{
		strURL = _T("https://api.themoviedb.org/3/search/movie?query=") + strEncoded +
			_T("&api_key=") + strAPIKey;
		if (!strYear.IsEmpty())
			strURL += _T("&year=") + strYear;
	}

	JsonDoc doc;
	if (!TMDBRequest(strURL, doc))
		return DBI_STATUS_CONNERROR;

	int nCount = doc.GetArrLen(L"results");
	if (nCount == 0)
		return DBI_STATUS_UNKNOWN;

	bool bIsTV = (bType == DB_TYPE_TV);
	if (!TMDBPickBestResult(doc, strTitle, strYear, bType, bIsTV, strBestID))
		return DBI_STATUS_UNKNOWN;

	return DBI_STATUS_UPDATED;
}

static int TMDBFindByIMDbID(RString strAPIKey, RString strIMDbID, BYTE bType, RString &strBestID)
{
	if (strIMDbID.IsEmpty()) return DBI_STATUS_UNKNOWN;

	RString strURL = _T("https://api.themoviedb.org/3/find/") + strIMDbID +
		_T("?api_key=") + strAPIKey + _T("&external_source=imdb_id");

	JsonDoc doc;
	if (!TMDBRequest(strURL, doc))
		return DBI_STATUS_CONNERROR;

	if (bType == DB_TYPE_TV)
	{
		int arrIdx = doc.FindKey(doc.RootIdx(), L"tv_results");
		if (arrIdx < 0 || doc.NCount(arrIdx) == 0) return DBI_STATUS_UNKNOWN;
		int itemIdx = doc.NAt(arrIdx, 0);
		if (itemIdx < 0) return DBI_STATUS_UNKNOWN;
		strBestID = NumberToString(doc.NGetInt(itemIdx, L"id", 0));
	}
	else
	{
		int arrIdx = doc.FindKey(doc.RootIdx(), L"movie_results");
		if (arrIdx < 0 || doc.NCount(arrIdx) == 0) return DBI_STATUS_UNKNOWN;
		int itemIdx = doc.NAt(arrIdx, 0);
		if (itemIdx < 0) return DBI_STATUS_UNKNOWN;
		strBestID = NumberToString(doc.NGetInt(itemIdx, L"id", 0));
	}

	return strBestID.IsEmpty() ? DBI_STATUS_UNKNOWN : DBI_STATUS_UPDATED;
}

static void TMDBParseMovieDetails(JsonDoc &doc, DBINFO *pInfo, bool bIsTV)
{
	std::wstring titleKey = bIsTV ? L"name" : L"title";
	std::wstring dateKey = bIsTV ? L"first_air_date" : L"release_date";

	pInfo->strTitle = doc.GetStr(titleKey).c_str();
	pInfo->strYear = doc.GetStr(dateKey).c_str();

	RString strPosterPath = doc.GetStr(L"poster_path").c_str();
	if (!strPosterPath.IsEmpty())
	{
		RString strPosterURL = TMDBImageURL(strPosterPath, _T("w500"));
		URLToData(strPosterURL, pInfo->posterData);
	}

	pInfo->fRating = (float)doc.GetDbl(L"vote_average", 0);
	pInfo->fRatingMax = 10.0f;
	pInfo->nVotes = doc.GetInt(L"vote_count", 0);

	pInfo->strStoryline = doc.GetStr(L"overview").c_str();

	if (!bIsTV)
	{
		pInfo->nRuntime = doc.GetInt(L"runtime", 0);
		pInfo->strIMDbID = doc.GetStr(L"imdb_id").c_str();
	}
	else
	{
		int epRuntime = 0;
		int arrIdx = doc.FindKey(doc.RootIdx(), L"episode_run_time");
		if (arrIdx >= 0 && doc.NCount(arrIdx) > 0)
		{
			JsonVal *pVal = doc.Node(doc.NAt(arrIdx, 0));
			if (pVal && pVal->type == JsonVal::Number)
				epRuntime = (int)pVal->num;
		}
		pInfo->nRuntime = epRuntime;

		int extIdx = doc.FindKey(doc.RootIdx(), L"external_ids");
		if (extIdx >= 0)
			pInfo->strIMDbID = doc.NGetStr(extIdx, L"imdb_id").c_str();
	}

	if (pInfo->strYear.Find(_T('-')) == 4)
		pInfo->strYear = pInfo->strYear.Left(4);

	RString strGenres;
	int genresIdx = doc.FindKey(doc.RootIdx(), L"genres");
	if (genresIdx >= 0)
	{
		int gCount = doc.NCount(genresIdx);
		for (int i = 0; i < gCount; i++)
		{
			int gIdx = doc.NAt(genresIdx, i);
			if (gIdx < 0) continue;
			RString strG = doc.NGetStr(gIdx, L"name").c_str();
			if (!strG.IsEmpty())
			{
				if (!strGenres.IsEmpty()) strGenres += _T("|");
				strGenres += strG;
			}
		}
	}
	pInfo->strGenres = strGenres;

	RString strCountries;
	int countriesIdx = doc.FindKey(doc.RootIdx(), L"production_countries");
	if (countriesIdx >= 0)
	{
		int cCount = doc.NCount(countriesIdx);
		for (int i = 0; i < cCount; i++)
		{
			int cIdx = doc.NAt(countriesIdx, i);
			if (cIdx < 0) continue;
			RString strC = doc.NGetStr(cIdx, L"name").c_str();
			if (!strC.IsEmpty())
			{
				if (!strCountries.IsEmpty()) strCountries += _T("|");
				strCountries += strC;
			}
		}
	}
	pInfo->strCountries = strCountries;

	if (bIsTV)
	{
		int crIdx = doc.FindKey(doc.RootIdx(), L"content_ratings");
		if (crIdx >= 0)
		{
			int resultsIdx = doc.FindKey(crIdx, L"results");
			if (resultsIdx >= 0)
			{
				int rCount = doc.NCount(resultsIdx);
				for (int i = 0; i < rCount; i++)
				{
					int rIdx = doc.NAt(resultsIdx, i);
					if (rIdx < 0) continue;
					RString iso = doc.NGetStr(rIdx, L"iso_3166_1").c_str();
					if (iso == _T("US"))
					{
						pInfo->strContentRating = doc.NGetStr(rIdx, L"rating").c_str();
						break;
					}
				}
			}
		}
	}
	else
	{
		int rdIdx = doc.FindKey(doc.RootIdx(), L"release_dates");
		if (rdIdx >= 0)
		{
			int resultsIdx = doc.FindKey(rdIdx, L"results");
			if (resultsIdx >= 0)
			{
				int rCount = doc.NCount(resultsIdx);
				for (int i = 0; i < rCount; i++)
				{
					int rIdx = doc.NAt(resultsIdx, i);
					if (rIdx < 0) continue;
					RString iso = doc.NGetStr(rIdx, L"iso_3166_1").c_str();
					if (iso == _T("US"))
					{
						int datesIdx = doc.FindKey(rIdx, L"release_dates");
						if (datesIdx >= 0)
						{
							int dCount = doc.NCount(datesIdx);
							for (int j = 0; j < dCount; j++)
							{
								int dIdx = doc.NAt(datesIdx, j);
								if (dIdx < 0) continue;
								RString cert = doc.NGetStr(dIdx, L"certification").c_str();
								if (!cert.IsEmpty())
								{
									pInfo->strContentRating = cert;
									break;
								}
							}
						}
						break;
					}
				}
			}
		}
	}
}

static void TMDBParseCredits(JsonDoc &doc, DBINFO *pInfo)
{
	int creditsIdx = doc.FindKey(doc.RootIdx(), L"credits");
	if (creditsIdx < 0) return;

	RString strDirectors, strWriters, strStars;
	RString strActorPaths[DBI_STAR_NUMBER];

	int castIdx = doc.FindKey(creditsIdx, L"cast");
	if (castIdx >= 0)
	{
		int cCount = doc.NCount(castIdx);
		int nStarCount = 0;
		for (int i = 0; i < cCount && nStarCount < 30; i++)
		{
			int cIdx = doc.NAt(castIdx, i);
			if (cIdx < 0) continue;
			RString strName = doc.NGetStr(cIdx, L"name").c_str();
			if (strName.IsEmpty()) continue;
			if (!strStars.IsEmpty()) strStars += _T("|");
			strStars += strName;
			if (nStarCount < DBI_STAR_NUMBER)
			{
				RString strPath = doc.NGetStr(cIdx, L"profile_path").c_str();
				strActorPaths[nStarCount] = strPath;
				pInfo->strActorId[nStarCount] = NumberToString(doc.NGetInt(cIdx, L"id", 0));
			}
			nStarCount++;
		}
	}

	int crewIdx = doc.FindKey(creditsIdx, L"crew");
	if (crewIdx >= 0)
	{
		int cCount = doc.NCount(crewIdx);
		for (int i = 0; i < cCount; i++)
		{
			int cIdx = doc.NAt(crewIdx, i);
			if (cIdx < 0) continue;
			RString strName = doc.NGetStr(cIdx, L"name").c_str();
			RString strJob = doc.NGetStr(cIdx, L"job").c_str();
			RString strDept = doc.NGetStr(cIdx, L"department").c_str();

			if (strJob == _T("Director"))
			{
				if (!strDirectors.IsEmpty()) strDirectors += _T("|");
				strDirectors += strName;
			}
			else if (strDept == _T("Writing") &&
				(strJob == _T("Screenplay") || strJob == _T("Writer") || strJob == _T("Story")))
			{
				if (!strWriters.IsEmpty()) strWriters += _T("|");
				strWriters += strName;
			}
		}
	}

	pInfo->strDirectors = strDirectors;
	pInfo->strWriters = strWriters;
	pInfo->strStars = strStars;

	for (int i = 0; i < DBI_STAR_NUMBER; i++)
	{
		if (!strActorPaths[i].IsEmpty())
		{
			RString strURL = TMDBImageURL(strActorPaths[i], _T("w185"));
			RArray<BYTE> arData;
			if (URLToData(strURL, arData) && arData.GetSize() > 0)
			{
				RString strStarName = GetStar(pInfo->strStars, i);
				if (!strStarName.IsEmpty())
					GetImageHash()->SetImage(strStarName, arData);
				pInfo->actorImageData[i] = GetImageHash()->GetImage(strStarName);
			}
		}
	}
}

static void TMDBParseSeason(JsonDoc &doc, DBINFO *pInfo, std::map<RString, SeriesCache> *pSeriesCache, RString strSeriesID)
{
	int epArrIdx = doc.FindKey(doc.RootIdx(), L"episodes");
	if (epArrIdx < 0) return;

	SeriesSeasonData seasonData;
	int epCount = doc.NCount(epArrIdx);

	for (int i = 0; i < epCount; i++)
	{
		int epIdx = doc.NAt(epArrIdx, i);
		if (epIdx < 0) continue;

		RString strEpName = doc.NGetStr(epIdx, L"name").c_str();
		RString strEpID = NumberToString(doc.NGetInt(epIdx, L"id", 0));
		RString strEpRating = FloatToString((float)doc.NGetDbl(epIdx, L"vote_average", 0));
		RString strEpVotes = NumberToString(doc.NGetInt(epIdx, L"vote_count", 0));
		RString strEpAir = doc.NGetStr(epIdx, L"air_date").c_str();

		seasonData.episodeTitles.Add(strEpName);
		seasonData.episodeIDs.Add(strEpID);
		seasonData.episodeRatings.Add(strEpRating);
		seasonData.episodeVotes.Add(strEpVotes);
		seasonData.episodeReleased.Add(strEpAir);
	}

	if (pSeriesCache && !strSeriesID.IsEmpty())
		(*pSeriesCache)[strSeriesID].seasons[pInfo->nSeason] = seasonData;

	if (pInfo->nEpisode >= 1 && pInfo->nEpisode <= (INT_PTR)seasonData.episodeTitles.GetSize())
	{
		int idx = pInfo->nEpisode - 1;
		pInfo->strEpisodeName = seasonData.episodeTitles[idx];
		pInfo->strEpisodeID = seasonData.episodeIDs[idx];
		pInfo->fRating = (float)StringToFloat(seasonData.episodeRatings[idx]);
		pInfo->fRatingMax = 10.0f;
		pInfo->nVotes = StringToNumber(seasonData.episodeVotes[idx]);
		pInfo->strAirDate = seasonData.episodeReleased[idx];
	}
}

static void TMDBFetchSeason(RString strAPIKey, RString strSeriesID, DBINFO *pInfo,
	std::map<RString, SeriesCache> *pSeriesCache)
{
	if (pInfo->nSeason < 0) return;

	if (pSeriesCache && !strSeriesID.IsEmpty())
	{
		auto it = pSeriesCache->find(strSeriesID);
		if (it != pSeriesCache->end())
		{
			auto sit = it->second.seasons.find(pInfo->nSeason);
			if (sit != it->second.seasons.end())
			{
				SeriesSeasonData &sd = sit->second;
				if (pInfo->nEpisode >= 1 && pInfo->nEpisode <= (INT_PTR)sd.episodeTitles.GetSize())
				{
					int idx = pInfo->nEpisode - 1;
					pInfo->strEpisodeName = sd.episodeTitles[idx];
					pInfo->strEpisodeID = sd.episodeIDs[idx];
					pInfo->fRating = (float)StringToFloat(sd.episodeRatings[idx]);
					pInfo->fRatingMax = 10.0f;
					pInfo->nVotes = StringToNumber(sd.episodeVotes[idx]);
					pInfo->strAirDate = sd.episodeReleased[idx];
				}
				return;
			}
		}
	}

	RString strURL = _T("https://api.themoviedb.org/3/tv/") + strSeriesID +
		_T("/season/") + NumberToString(pInfo->nSeason) +
		_T("?api_key=") + strAPIKey;

	JsonDoc doc;
	if (!TMDBRequest(strURL, doc)) return;

	TMDBParseSeason(doc, pInfo, pSeriesCache, strSeriesID);
}

static DWORD TMDBFetchRatingsFromOMDb(DBINFO *pInfo, RString strOMDbAPIKey, OMDbUsageTracker *pUsage)
{
	if (pInfo->strIMDbID.IsEmpty()) return DBI_STATUS_UNKNOWN;
	if (pInfo->strIMDbID.Left(2) != _T("tt")) return DBI_STATUS_UNKNOWN;
	if (pUsage && !pUsage->RequestAllowed()) return DBI_STATUS_RATELIMITED;

	RString strURL = _T("https://www.omdbapi.com/?apikey=") + strOMDbAPIKey +
		_T("&i=") + pInfo->strIMDbID + _T("&plot=full&r=xml");

	RXMLFile2 xmlFile;
	RString strResponse;
	if (!URLToString(strURL, strResponse) || strResponse.IsEmpty())
		return DBI_STATUS_CONNERROR;

	if (pUsage)
		pUsage->Increment();

	if (!xmlFile.OpenFromStr(strResponse))
		return DBI_STATUS_SCRAPEERROR;

	const RXMLElem2 &root = xmlFile.GetRootElem();
	RString strResponseAttr = root.GetAttribute(_T("response"));
	if (_tcsicmp(strResponseAttr, _T("false")) == 0)
	{
		RString strError = root.GetAttribute(_T("error"));
		if (strError.FindNoCase(_T("limit")) >= 0)
			return DBI_STATUS_RATELIMITED;
		return DBI_STATUS_UNKNOWN;
	}

	const RArray<RXMLElem2*> &children = root.GetChildren();
	for (INT_PTR i = 0; i < children.GetSize(); ++i)
	{
		if (children[i]->GetName() == _T("movie"))
		{
			RString strIMDbRating = children[i]->GetAttribute(_T("imdbRating"));
			RString strIMDbVotes = children[i]->GetAttribute(_T("imdbVotes"));
			RString strMetascore = children[i]->GetAttribute(_T("metascore"));

			if (!strIMDbRating.IsEmpty() && strIMDbRating != _T("N/A"))
			{
				pInfo->fIMDbRating = (float)StringToFloat(strIMDbRating);
				pInfo->fIMDbRatingMax = 10.0f;
			}
			if (!strIMDbVotes.IsEmpty() && strIMDbVotes != _T("N/A"))
			{
				RString strVotes = strIMDbVotes;
				strVotes.Replace(_T(","), _T(""));
				pInfo->nIMDbVotes = StringToNumber(strVotes);
			}
			if (!strMetascore.IsEmpty() && strMetascore != _T("N/A"))
			{
				pInfo->nMetascore = StringToNumber(strMetascore);
			}

			return DBI_STATUS_UPDATED;
		}
	}

	return DBI_STATUS_UNKNOWN;
}

DWORD ScrapeTMDB(DBINFO *pInfo, RString strTMDBAPIKey, RString strOMDbAPIKey,
	std::map<RString, SeriesCache> *pSeriesCache, OMDbUsageTracker *pUsage)
{
	if (!pInfo) return DBI_STATUS_SCRAPEERROR;
	if (strTMDBAPIKey.IsEmpty()) return DBI_STATUS_SCRAPEERROR;

	bool bIsTV = (pInfo->bType == DB_TYPE_TV);
	RString strTMDBID;

	if (!pInfo->strID.IsEmpty() && pInfo->strID[0] != _T('t'))
	{
		strTMDBID = pInfo->strID;
	}
	else if (!pInfo->strIMDbID.IsEmpty() && pInfo->strIMDbID.Left(2) == _T("tt"))
	{
		TMDBFindByIMDbID(strTMDBAPIKey, pInfo->strIMDbID, pInfo->bType, strTMDBID);
	}

	if (strTMDBID.IsEmpty() && !pInfo->strSearchTitle.IsEmpty())
	{
		RString strBestID;
		int nResult = TMDBSearch(strTMDBAPIKey, pInfo->strSearchTitle, pInfo->strSearchYear, pInfo->bType, strBestID);
		if (nResult == DBI_STATUS_CONNERROR) return DBI_STATUS_CONNERROR;
		if (nResult == DBI_STATUS_UPDATED)
			strTMDBID = strBestID;
	}

	if (strTMDBID.IsEmpty())
	{
		if (!pInfo->strSearchTitle.IsEmpty())
		{
			RString strAltTitle = pInfo->strSearchTitle;
			INT_PTR nDash = strAltTitle.Find(_T(" - "));
			if (nDash > 0)
			{
				RString strLeft = strAltTitle.Left(nDash);
				RString strRight = strAltTitle.Mid(nDash + 3);
				static const TCHAR *genres[] = {
					_T("Documentary"), _T("Short"), _T("Animation"), _T("Music"),
					_T("Horror"), _T("Thriller"), _T("Comedy"), _T("Drama"),
					_T("Action"), _T("Adventure"), _T("Fantasy"), _T("Sci-Fi"),
					_T("Science Fiction"), _T("Crime"), _T("Mystery"), _T("Romance")
				};
				bool bRightIsGenre = false;
				for (int i = 0; i < sizeof(genres)/sizeof(genres[0]); i++)
				{
					if (_tcsicmp(strRight, genres[i]) == 0)
					{ bRightIsGenre = true; break; }
				}
				if (bRightIsGenre)
				{
					RString strBestID;
					if (TMDBSearch(strTMDBAPIKey, strLeft, pInfo->strSearchYear, pInfo->bType, strBestID) == DBI_STATUS_UPDATED)
						strTMDBID = strBestID;
				}
			}
		}

		if (strTMDBID.IsEmpty())
		{
			if (!pInfo->strSearchYear.IsEmpty() && !bIsTV)
			{
				RString strBestID;
				if (TMDBSearch(strTMDBAPIKey, pInfo->strSearchTitle, _T(""), pInfo->bType, strBestID) == DBI_STATUS_UPDATED)
					strTMDBID = strBestID;
			}

			if (strTMDBID.IsEmpty())
				return DBI_STATUS_UNKNOWN;
		}
	}

	pInfo->strID = strTMDBID;

	RString strURL;
	if (bIsTV)
		strURL = _T("https://api.themoviedb.org/3/tv/") + strTMDBID +
			_T("?api_key=") + strTMDBAPIKey +
			_T("&append_to_response=credits,content_ratings,external_ids");
	else
		strURL = _T("https://api.themoviedb.org/3/movie/") + strTMDBID +
			_T("?api_key=") + strTMDBAPIKey +
			_T("&append_to_response=credits,release_dates");

	JsonDoc doc;
	if (!TMDBRequest(strURL, doc))
		return DBI_STATUS_CONNERROR;

	TMDBParseMovieDetails(doc, pInfo, bIsTV);
	TMDBParseCredits(doc, pInfo);

	if (pInfo->strTitle.GetLength() >= 4 && pInfo->strTitle.Left(4) == _T("The "))
		pInfo->strTitle = pInfo->strTitle.Mid(4) + _T(", The");
	else if (pInfo->strTitle.GetLength() >= 2 && pInfo->strTitle.Left(2) == _T("A "))
		pInfo->strTitle = pInfo->strTitle.Mid(2) + _T(", A");

	if (bIsTV && pInfo->nSeason >= 0)
		TMDBFetchSeason(strTMDBAPIKey, strTMDBID, pInfo, pSeriesCache);

	if (!pInfo->bOMDbRatingsFetched && !strOMDbAPIKey.IsEmpty())
	{
		DWORD omdbResult = TMDBFetchRatingsFromOMDb(pInfo, strOMDbAPIKey, pUsage);
		if (omdbResult == DBI_STATUS_UPDATED || omdbResult == DBI_STATUS_UNKNOWN)
			pInfo->bOMDbRatingsFetched = true;
	}

	return DBI_STATUS_UPDATED;
}
