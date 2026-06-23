#include "stdafx.h"
#include "MovieExplorer.h"
#include "UpdateThread.h"
#include "ParseFileName.h"
#include "ScrapeIMDb.h"
#include "ScrapeMovieMeter.h"

static RString GetCacheFileName(RString strCacheDir, RString strServ, RString strID, INT_PTR nSeason, INT_PTR nEpisode)
{
	RString strFileName = strCacheDir + _T("\\") + strServ + _T("\\") + strID;
	if (nSeason >= 0 && nEpisode >= 0)
		strFileName += _T("_S") + NumberToString(nSeason) + _T("_E") + NumberToString(nEpisode);
	return strFileName;
}

UINT CALLBACK UpdateThread(void *pParam)
{
	HWND hDatabaseWnd = ((UPDATETHREADDATA*)pParam)->hDatabaseWnd;
	SeriesDedup *pDedup = ((UPDATETHREADDATA*)pParam)->pDedup;
	OMDbUsageTracker *pUsage = ((UPDATETHREADDATA*)pParam)->pUsage;

	RString strOMDbAPIKey = GETPREFSTR(_T("OMDbAPIKey"));

	UINT64 nWeeks = (UINT64)GETPREFINT(_T("Database"), _T("MaxInfoAge"));
	if (nWeeks < 2)
		nWeeks = 2;
	UINT64 maxTimeDiff = nWeeks * 7 * 24 * 60 * 60 * 10000000;
	UINT64 currentTime = GetSystemTime();
	UINT64 minTime = currentTime - maxTimeDiff;

	RString strCacheDir = CorrectPath(GETPREFSTR(_T("Database"), _T("CacheDirectory")));

	RObArray<RString> servicesInUse;
	RString strOnlyUse = GETPREFSTR(_T("InfoService"), _T("OnlyUse"));
	if (!strOnlyUse.IsEmpty())
		servicesInUse.Add(strOnlyUse);
	else
	{
		servicesInUse.Add(_T("imdb.com"));
		if (!GETPREFSTR(_T("InfoService"), _T("Title")).IsEmpty() &&
			GETPREFSTR(_T("InfoService"), _T("Title")) != _T("imdb.com"))
			servicesInUse.Add(GETPREFSTR(_T("InfoService"), _T("Title")));
	}

	foreach (servicesInUse, strServ, i)
		if (strServ.IsEmpty())
			{servicesInUse.RemoveAt(i); break;}

	MSG msg;
	PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

	((UPDATETHREADDATA*)pParam)->eReady.SetEvent();

	DBMOVIE mov;
	DBMOVIE *pOrigMov;
	DBINFO info;
	RString strSearchTitle, strSearchYear, strID, strAirDate;
	RXMLFile xmlFile;
	RXMLTag *pInfoTag;
	INT_PTR nUpdatedFromWeb = 0;
	INT_PTR nSeason = -1;
	INT_PTR nEpisode = -1;
	BYTE bType = DB_TYPE_UNKNOWN;
	bool bRateLimited = false;

	std::map<RString, SeriesCache> seriesCache;

	while (SendMessage(hDatabaseWnd, DBM_GETMOVIEUPDATE, (WPARAM)&mov, (LPARAM)&pOrigMov))
	{
		nSeason = -1; nEpisode = -1; strAirDate.Empty(); bType = DB_TYPE_UNKNOWN;
		RString strFullPath;
		if (mov.pDirectory)
			strFullPath = mov.pDirectory->strPath + _T("\\") + mov.strFileName;
		ParseFileName(mov.strFileName, strFullPath, strSearchTitle, strSearchYear, nSeason, nEpisode, strAirDate, bType);

		if (bRateLimited)
		{
			SendMessage(hDatabaseWnd, DBM_SETMOVIEUPDATE, (WPARAM)&mov, (LPARAM)pOrigMov);
			continue;
		}

		foreach (servicesInUse, strServ)
		{
			ClearInfo(&info);

			if (strServ == _T("imdb.com"))
				strID = mov.strIMDbID;
			else if (strServ == _T("moviemeter.nl"))
				strID = mov.strMovieMeterID;
			else
				ASSERT(false);

			if (strID == _T("unknown") || strID == _T("connError") || strID == _T("scrapeError") || strID == _T("rateLimited"))
				continue;

			// Check dedup map for TV series search string -> series ID

			RString strDedupKey;
			if (strServ == _T("imdb.com") && strID.IsEmpty() && bType == DB_TYPE_TV)
			{
				strDedupKey = strSearchTitle + _T("|") + strSearchYear + _T("|") + NumberToString(bType);
				RString strFoundID;
				if (pDedup)
					strFoundID = pDedup->Lookup(strDedupKey);
				if (!strFoundID.IsEmpty())
					strID = strFoundID;
			}

			// Try to update from cache when ID is provided

			if (!strID.IsEmpty())
			{
				RString strCacheFile = GetCacheFileName(strCacheDir, strServ, strID, nSeason, nEpisode);
				bool bCacheRead = (xmlFile.Read(strCacheFile + _T(".xml")) != 0);
				if (!bCacheRead && nSeason >= 0 && nEpisode >= 0)
					bCacheRead = (xmlFile.Read(strCacheDir + _T("\\") + strServ + _T("\\") + strID + _T(".xml")) != 0);
				if (bCacheRead)
				{
					pInfoTag = xmlFile.GetRootTag()->GetChild(_T("MovieInfo"));
					if (pInfoTag)
					{
						ASSERT(pInfoTag->GetChildContent(_T("Timestamp")));
						UINT64 timestamp = StringToNumber64(pInfoTag->GetChildContent(_T("Timestamp")));
						if (timestamp >= minTime)
						{
							TagToInfo(pInfoTag, &info);

							RString strPosterID = info.strID.IsEmpty() ? strID : info.strID;
							if (nSeason >= 0 && nEpisode >= 0 && !info.strEpisodeID.IsEmpty())
								strPosterID = strID;
							VERIFY(FileToData(strCacheDir + _T("\\") + strServ + _T("\\") + strPosterID +
									_T(".jpg"), info.posterData));

							for (int i = 0; i < DBI_STAR_NUMBER; i++)
							{
								RString strStarName = GetStar(info.strStars, i);
								info.actorImageData[i] = GetImageHash()->GetImage(strStarName);

								if (!info.actorImageData[i] && !strStarName.IsEmpty() && FileExists(strCacheDir + _T("\\") +
									strServ + _T("\\actors\\") + strStarName + _T(".jpg")))
								{
									RArray<BYTE> arTmp;
									if (FileToData(strCacheDir + _T("\\") + strServ + _T("\\actors\\")
										+ strStarName + _T(".jpg"), arTmp))
									{
										GetImageHash()->SetImage(strStarName, arTmp);
										info.actorImageData[i] = GetImageHash()->GetImage(strStarName);
									}
								}
							}

							info.status = DBI_STATUS_UPDATED;
						}
					}
				}
			}

			// Otherwise update from the web

			if (info.status != DBI_STATUS_UPDATED)
			{
				info.strServiceName = strServ;
				info.strSearchTitle = strSearchTitle;
				info.strSearchYear = strSearchYear;
				info.nSeason = nSeason;
				info.nEpisode = nEpisode;
				info.strAirDate = strAirDate;
				info.bType = bType;
				info.strID = strID;

				if (strServ == _T("imdb.com"))
					info.status = ScrapeIMDb(&info, strOMDbAPIKey, &seriesCache, pUsage);
				else if (strServ == _T("moviemeter.nl"))
					info.status = ScrapeMovieMeter(&info);
				else
					ASSERT(false);

				if (PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_NOREMOVE))
					return 0;

				if (info.status == DBI_STATUS_RATELIMITED)
				{
					LOG(_T("OMDb API rate limit reached. Stopping web updates.\n"));
					bRateLimited = true;
					if (strID.IsEmpty() || (strID.GetLength() < 2 || strID.Left(2) != _T("tt")))
						strID = _T("rateLimited");
					break;
				}

				if (info.status == DBI_STATUS_UPDATED)
				{
					strID = info.strID;
					LOG(_T("Succesfully updated ") + info.strTitle + _T(" (") + info.strYear +
						_T(") from ") + strServ + _T(".\n"));

					if (pDedup && !strDedupKey.IsEmpty())
						pDedup->Store(strDedupKey, strID);

					++nUpdatedFromWeb;

					info.timestamp = GetSystemTime();
					xmlFile.GetRootTag()->RemoveAllChildren();
					InfoToTag(&info, xmlFile.GetRootTag()->AddChild(_T("MovieInfo")));
					if (!DirectoryExists(strCacheDir))
						CreateDirectory(strCacheDir);
					if (!DirectoryExists(strCacheDir + _T("\\") + strServ))
						CreateDirectory(strCacheDir + _T("\\") + strServ);

					RString strCacheBase = GetCacheFileName(strCacheDir, strServ, strID, nSeason, nEpisode);
					VERIFY(xmlFile.Write(strCacheBase + _T(".xml")));

					VERIFY(DataToFile(info.posterData, strCacheDir + _T("\\") + strServ +
							_T("\\") + strID + _T(".jpg")));

					if (!DirectoryExists(strCacheDir + _T("\\") + strServ + _T("\\actors\\")))
						CreateDirectory(strCacheDir + _T("\\") + strServ + _T("\\actors\\"));

					for (int i = 0; i < DBI_STAR_NUMBER; i++)
					{
						RString strStarName = GetStar(info.strStars, i);
						if (!strStarName.IsEmpty() && info.actorImageData[i] &&
							!FileExists(strCacheDir + _T("\\") + strServ + _T("\\actors\\")
							+ strStarName + _T(".jpg")))
							VERIFY(DataToFile(*info.actorImageData[i], strCacheDir + _T("\\") + strServ + _T("\\actors\\")
							+ strStarName + _T(".jpg")));
					}
				}
				else if (info.status == DBI_STATUS_UNKNOWN)
				{
					strID = _T("unknown");
					LOG(_T("Failed to identify '") + mov.strFileName + _T("' on ") +
							strServ + _T(".\n"));
				}
				else if (info.status == DBI_STATUS_CONNERROR)
				{
					strID = _T("connError");
					LOG(_T("A connection error occured while identifying ") + mov.strFileName +
							_T(" on ") + strServ + _T(".\n"));
				}
				else if (info.status == DBI_STATUS_SCRAPEERROR)
				{
					strID = _T("scrapeError");
					LOG(_T("A parsing error occured while identifying ") + mov.strFileName +
							_T(" on ") + strServ + _T(".\n"));
				}
			}

			// Assign values to movie object

			if (info.status == DBI_STATUS_UPDATED)
			{
				mov.strTitle = info.strTitle;
				mov.strYear = info.strYear;
				mov.nYear = StringToNumber(mov.strYear);
				if (mov.nYear == 0 && !mov.strYear.IsEmpty())
				{
					INT_PTR nDash = mov.strYear.Find(_T('-'));
					if (nDash > 0)
						mov.nYear = StringToNumber(mov.strYear.Left(nDash));
				}
				mov.strCountries = info.strCountries;
				mov.strGenres = info.strGenres;
				mov.nRuntime = info.nRuntime;
				mov.strStoryline = info.strStoryline;
				mov.strDirectors = info.strDirectors;
				mov.strWriters = info.strWriters;
				mov.strStars = info.strStars;
				mov.posterData = info.posterData;
				mov.fRating = info.fRating;
				mov.fRatingMax = info.fRatingMax;
				mov.nMetascore = info.nMetascore;
				mov.nVotes = info.nVotes;

				if (mov.fIMDbRating == 0.0f && info.fIMDbRating != 0.0f)
				{
					if (mov.strIMDbID.IsEmpty() || (mov.strIMDbID == info.strIMDbID))
					{
						mov.strIMDbID = info.strIMDbID;
						mov.fIMDbRating = info.fIMDbRating;
						mov.fIMDbRatingMax = info.fIMDbRatingMax;
						mov.nIMDbVotes = info.nIMDbVotes;
					}
				}

				mov.strContentRating = info.strContentRating;
				mov.nSeason = info.nSeason;
				mov.nEpisode = info.nEpisode;
				mov.strEpisodeName = info.strEpisodeName;
				mov.strEpisodeID = info.strEpisodeID;
				mov.strAirDate = info.strAirDate;
				mov.bType = info.bType;
				for (int i = 0; i < DBI_STAR_NUMBER; i++)
				{
					mov.strActorId[i] = info.strActorId[i];
					mov.actorImageData[i] = info.actorImageData[i];
				}
			}

			// Save to the right ID

			if (strServ == _T("imdb.com"))
				mov.strIMDbID = strID;
			else if (strServ == _T("moviemeter.nl"))
				mov.strMovieMeterID = strID;
			else
				ASSERT(false);
		}

		if (!bRateLimited)
			mov.bUpdated = true;

		if (PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_NOREMOVE))
			return 0;

		SendMessage(hDatabaseWnd, DBM_SETMOVIEUPDATE, (WPARAM)&mov, (LPARAM)pOrigMov);

		if (PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_NOREMOVE))
			return 0;
	}

	PostMessage(hDatabaseWnd, DBM_UPDATETHREADEND, GetCurrentThreadId(), nUpdatedFromWeb);
	return 0;
}
