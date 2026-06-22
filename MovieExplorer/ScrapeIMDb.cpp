#include "stdafx.h"
#include "MovieExplorer.h"
#include "UpdateThread.h"

static RString CommaToPipe(RString str)
{
	str.Replace(_T(", "), _T("|"));
	return str;
}

static RString StripMinFromRuntime(RString str)
{
	RString strNum;
	INT_PTR n = str.Find(_T(' '));
	if (n > 0)
		strNum = str.Left(n);
	else
		strNum = str;
	return strNum;
}

static bool OMDbRequest(RString strURL, RXMLFile2 &xmlFile)
{
	RString strResponse;
	if (!URLToString(strURL, strResponse))
		return false;

	if (strResponse.IsEmpty())
		return false;

	if (!xmlFile.OpenFromStr(strResponse))
		return false;

	return true;
}

static bool OMDbIsResponseTrue(RXMLFile2 &xmlFile)
{
	const RXMLElem2 &root = xmlFile.GetRootElem();
	RString strResponse = root.GetAttribute(_T("response"));
	return _tcsicmp(strResponse, _T("true")) == 0;
}

static bool OMDbIsRateLimited(RXMLFile2 &xmlFile)
{
	const RXMLElem2 &root = xmlFile.GetRootElem();
	RString strResponse = root.GetAttribute(_T("response"));
	if (_tcsicmp(strResponse, _T("false")) == 0)
	{
		RString strError = root.GetAttribute(_T("error"));
		if (strError.FindNoCase(_T("limit")) >= 0)
			return true;
	}
	return false;
}

static RString OMDbBuildSearchURL(RString strAPIKey, RString strTitle, RString strYear, BYTE bType)
{
	RString strURL = _T("https://www.omdbapi.com/?apikey=") + strAPIKey +
		_T("&s=") + URLEncode(strTitle) +
		_T("&r=xml");

	if (!strYear.IsEmpty() && bType != DB_TYPE_TV)
		strURL += _T("&y=") + strYear;

	if (bType == DB_TYPE_TV)
		strURL += _T("&type=series");
	else if (bType == DB_TYPE_MOVIE)
		strURL += _T("&type=movie");

	return strURL;
}

static bool OMDbPickBestResult(RXMLFile2 &xmlFile, RString strSearchTitle, RString strSearchYear, BYTE bType, RString &strBestID)
{
	const RXMLElem2 &root = xmlFile.GetRootElem();
	const RArray<RXMLElem2*> &children = root.GetChildren();

	strBestID.Empty();
	INT_PTR nBestMatchScore = -1;

	for (INT_PTR i = 0; i < children.GetSize(); ++i)
	{
		if (children[i]->GetName() != _T("result"))
			continue;

		RString strResultTitle = children[i]->GetAttribute(_T("title"));
		RString strResultYear = children[i]->GetAttribute(_T("year"));
		RString strResultID = children[i]->GetAttribute(_T("imdbID"));
		RString strResultType = children[i]->GetAttribute(_T("type"));

		INT_PTR nScore = 0;

		if (_tcsicmp(strResultTitle, strSearchTitle) == 0)
			nScore += 100;
		else if (strResultTitle.FindNoCase(strSearchTitle) >= 0)
			nScore += 50;

		if (!strSearchYear.IsEmpty() && strResultYear == strSearchYear)
			nScore += 30;

		if (bType == DB_TYPE_TV && _tcsicmp(strResultType, _T("series")) == 0)
			nScore += 20;
		else if (bType == DB_TYPE_MOVIE && _tcsicmp(strResultType, _T("movie")) == 0)
			nScore += 20;

		if (nScore > nBestMatchScore)
		{
			nBestMatchScore = nScore;
			strBestID = strResultID;
		}
	}

	return !strBestID.IsEmpty();
}

static int OMDbSearch(RString strAPIKey, RString strTitle, RString strYear, BYTE bType, RString &strBestID)
{
	RString strURL = OMDbBuildSearchURL(strAPIKey, strTitle, strYear, bType);

	RXMLFile2 xmlFile;
	if (!OMDbRequest(strURL, xmlFile))
		return DBI_STATUS_CONNERROR;

	if (OMDbIsRateLimited(xmlFile))
		return DBI_STATUS_RATELIMITED;

	if (!OMDbIsResponseTrue(xmlFile))
		return DBI_STATUS_UNKNOWN;

	if (!OMDbPickBestResult(xmlFile, strTitle, strYear, bType, strBestID))
		return DBI_STATUS_UNKNOWN;

	return DBI_STATUS_UPDATED;
}

static RString TryApostropheVariants(RString strTitle)
{
	static const RString strPrefixes[] = {
		_T("Im "), _T("Ill "), _T("Ive "), _T("Id "), _T("Were "),
		_T("Theyre "), _T("Thats "), _T("Hes "), _T("Shes "), _T("Whos "),
		_T("Whats "), _T("Heres "), _T("Theres "), _T("Wheres "),
		_T("Cant "), _T("Wont "), _T("Dont "), _T("Isnt "), _T("Didnt "),
		_T("Wouldnt "), _T("Couldnt "), _T("Shouldnt "), _T("Hasnt "),
		_T("Havent "), _T("Hadnt "), _T("Wasnt "), _T("Werent "),
		_T("Arent "), _T("Doesnt ")
	};

	static const RString strPrefixReplacements[] = {
		_T("I'm "), _T("I'll "), _T("I've "), _T("I'd "), _T("We're "),
		_T("They're "), _T("That's "), _T("He's "), _T("She's "), _T("Who's "),
		_T("What's "), _T("Here's "), _T("There's "), _T("Where's "),
		_T("Can't "), _T("Won't "), _T("Don't "), _T("Isn't "), _T("Didn't "),
		_T("Wouldn't "), _T("Couldn't "), _T("Shouldn't "), _T("Hasn't "),
		_T("Haven't "), _T("Hadn't "), _T("Wasn't "), _T("Weren't "),
		_T("Aren't "), _T("Doesn't ")
	};

	for (INT_PTR i = 0; i < sizeof(strPrefixes)/sizeof(strPrefixes[0]); ++i)
	{
		if (strTitle.FindNoCase(strPrefixes[i]) == 0)
		{
			RString strRest = strTitle.Mid(strPrefixes[i].GetLength());
			return strPrefixReplacements[i] + strRest;
		}
	}

	RString strResult = strTitle;

	for (INT_PTR pos = 0; pos < (INT_PTR)strResult.GetLength() - 1; ++pos)
	{
		INT_PTR next = pos + 1;
		if ((strResult[pos] == _T('s') || strResult[pos] == _T('S')) &&
			(strResult[next] == _T(' ') || next == strResult.GetLength() - 1) &&
			pos > 0 && strResult[pos - 1] != _T(' '))
		{
			strResult = strResult.Left(pos) + _T("'") + strResult.Mid(pos);
			break;
		}
	}

	if (strResult.GetLength() >= 3)
	{
		for (INT_PTR pos = 0; pos < strResult.GetLength() - 2; ++pos)
		{
			if ((strResult[pos] == _T('O') || strResult[pos] == _T('o')) &&
				strResult[pos + 1] >= _T('A') && strResult[pos + 1] <= _T('Z') &&
				strResult[pos + 2] >= _T('a') && strResult[pos + 2] <= _T('z') &&
				(pos == 0 || strResult[pos - 1] == _T(' ')))
			{
				RString strAfter = strResult.Mid(pos + 1, 2);
				if (_tcsicmp(strAfter, _T("Of")) != 0 && _tcsicmp(strAfter, _T("Or")) != 0 &&
					_tcsicmp(strAfter, _T("On")) != 0)
				{
					strResult = strResult.Left(pos + 1) + _T("'") + strResult.Mid(pos + 1);
					break;
				}
			}
		}
	}

	return strResult;
}

static RString TryStripCountrySuffix(RString strTitle)
{
	static const RString strSuffixes[] = {
		_T(" US"), _T(" UK"), _T(" AU"), _T(" CA"), _T(" NZ")
	};

	for (INT_PTR i = 0; i < sizeof(strSuffixes)/sizeof(strSuffixes[0]); ++i)
	{
		INT_PTR len = strSuffixes[i].GetLength();
		if (strTitle.GetLength() > len &&
			_tcsicmp(strTitle.Right(len), strSuffixes[i]) == 0)
		{
			return strTitle.Left(strTitle.GetLength() - len);
		}
	}

	return strTitle;
}

static RString TryStripPart(RString strTitle)
{
	INT_PTR nPartPos = strTitle.FindNoCase(_T(" Part "));
	if (nPartPos <= 0)
		return strTitle;

	RString strAfterPart = strTitle.Mid(nPartPos + 6);

	INT_PTR nPartNum = 0;
	if (!strAfterPart.IsEmpty())
	{
		if (strAfterPart.FindNoCase(_T("I")) == 0)
		{
			RString strRoman = strAfterPart;
			INT_PTR nLen = 0;
			for (INT_PTR i = 0; i < strRoman.GetLength(); ++i)
			{
				if (strRoman[i] == _T('I') || strRoman[i] == _T('i'))
					++nLen;
				else
					break;
			}
			if (nLen > 0 && nLen <= 3 && (nLen == strRoman.GetLength() || strRoman[nLen] == _T(' ')))
				nPartNum = (int)nLen;
		}
		if (nPartNum == 0)
		{
			RString strNum;
			for (INT_PTR i = 0; i < strAfterPart.GetLength(); ++i)
			{
				if (strAfterPart[i] >= _T('0') && strAfterPart[i] <= _T('9'))
				{
					TCHAR sz[2] = { strAfterPart[i], 0 };
					strNum += sz;
				}
				else
					break;
			}
			if (!strNum.IsEmpty())
				nPartNum = StringToNumber(strNum);
		}
	}

	RString strBase = strTitle.Left(nPartPos);
	if (nPartNum > 0)
		return strBase + _T(" ") + NumberToString(nPartNum);

	return strBase;
}

DWORD ScrapeIMDb(DBINFO *pInfo, RString strOMDbAPIKey, std::map<RString, SeriesCache> *pSeriesCache)
{
	if (!pInfo || pInfo->strSearchTitle.IsEmpty() || pInfo->strServiceName != _T("imdb.com"))
		{ASSERT(false); return DBI_STATUS_SCRAPEERROR;}

	if (strOMDbAPIKey.IsEmpty())
		return DBI_STATUS_CONNERROR;

	if (pInfo->strID.IsEmpty())
	{
		RString strBestID;

		int nSearchResult = OMDbSearch(strOMDbAPIKey, pInfo->strSearchTitle, pInfo->strSearchYear, pInfo->bType, strBestID);
		if (nSearchResult == DBI_STATUS_RATELIMITED)
			return DBI_STATUS_RATELIMITED;
		bool bFound = (nSearchResult == DBI_STATUS_UPDATED);

		if (!bFound && !pInfo->strSearchYear.IsEmpty() && pInfo->bType != DB_TYPE_TV)
		{
			nSearchResult = OMDbSearch(strOMDbAPIKey, pInfo->strSearchTitle, RString(), pInfo->bType, strBestID);
			if (nSearchResult == DBI_STATUS_RATELIMITED)
				return DBI_STATUS_RATELIMITED;
			bFound = (nSearchResult == DBI_STATUS_UPDATED);
		}

		if (!bFound)
		{
			RString strApostrophe = TryApostropheVariants(pInfo->strSearchTitle);
			if (strApostrophe != pInfo->strSearchTitle)
			{
				nSearchResult = OMDbSearch(strOMDbAPIKey, strApostrophe, pInfo->strSearchYear, pInfo->bType, strBestID);
				if (nSearchResult == DBI_STATUS_RATELIMITED)
					return DBI_STATUS_RATELIMITED;
				bFound = (nSearchResult == DBI_STATUS_UPDATED);

				if (!bFound && !pInfo->strSearchYear.IsEmpty() && pInfo->bType != DB_TYPE_TV)
				{
					nSearchResult = OMDbSearch(strOMDbAPIKey, strApostrophe, RString(), pInfo->bType, strBestID);
					if (nSearchResult == DBI_STATUS_RATELIMITED)
						return DBI_STATUS_RATELIMITED;
					bFound = (nSearchResult == DBI_STATUS_UPDATED);
				}
			}
		}

		if (!bFound && pInfo->bType == DB_TYPE_TV)
		{
			RString strStripped = TryStripCountrySuffix(pInfo->strSearchTitle);
			if (strStripped != pInfo->strSearchTitle)
			{
				nSearchResult = OMDbSearch(strOMDbAPIKey, strStripped, pInfo->strSearchYear, pInfo->bType, strBestID);
				if (nSearchResult == DBI_STATUS_RATELIMITED)
					return DBI_STATUS_RATELIMITED;
				bFound = (nSearchResult == DBI_STATUS_UPDATED);

				if (!bFound && !pInfo->strSearchYear.IsEmpty())
				{
					nSearchResult = OMDbSearch(strOMDbAPIKey, strStripped, RString(), pInfo->bType, strBestID);
					if (nSearchResult == DBI_STATUS_RATELIMITED)
						return DBI_STATUS_RATELIMITED;
					bFound = (nSearchResult == DBI_STATUS_UPDATED);
				}
			}
		}

		if (!bFound)
		{
			RString strStrippedPart = TryStripPart(pInfo->strSearchTitle);
			if (strStrippedPart != pInfo->strSearchTitle)
			{
				nSearchResult = OMDbSearch(strOMDbAPIKey, strStrippedPart, pInfo->strSearchYear, pInfo->bType, strBestID);
				if (nSearchResult == DBI_STATUS_RATELIMITED)
					return DBI_STATUS_RATELIMITED;
				bFound = (nSearchResult == DBI_STATUS_UPDATED);

				if (!bFound && !pInfo->strSearchYear.IsEmpty())
				{
					nSearchResult = OMDbSearch(strOMDbAPIKey, strStrippedPart, RString(), pInfo->bType, strBestID);
					if (nSearchResult == DBI_STATUS_RATELIMITED)
						return DBI_STATUS_RATELIMITED;
					bFound = (nSearchResult == DBI_STATUS_UPDATED);
				}
			}
		}

		if (!bFound)
			return DBI_STATUS_UNKNOWN;

		pInfo->strID = strBestID;
	}

	RString strURL = _T("https://www.omdbapi.com/?apikey=") + strOMDbAPIKey +
		_T("&i=") + pInfo->strID +
		_T("&plot=full&r=xml");

	RXMLFile2 xmlFile;
	if (!OMDbRequest(strURL, xmlFile))
		return DBI_STATUS_CONNERROR;

	if (OMDbIsRateLimited(xmlFile))
		return DBI_STATUS_RATELIMITED;

	if (!OMDbIsResponseTrue(xmlFile))
		return DBI_STATUS_UNKNOWN;

	const RXMLElem2 &root = xmlFile.GetRootElem();
	const RArray<RXMLElem2*> &children = root.GetChildren();

	const RXMLElem2 *pMovie = NULL;
	for (INT_PTR i = 0; i < children.GetSize(); ++i)
	{
		if (children[i]->GetName() == _T("movie"))
		{
			pMovie = children[i];
			break;
		}
	}

	if (!pMovie)
		return DBI_STATUS_SCRAPEERROR;

	RString strType = pMovie->GetAttribute(_T("type"));
	if (_tcsicmp(strType, _T("series")) == 0)
		pInfo->bType = DB_TYPE_TV;
	else if (_tcsicmp(strType, _T("episode")) == 0)
		pInfo->bType = DB_TYPE_TV;
	else
		pInfo->bType = DB_TYPE_MOVIE;

	if (pInfo->bType == DB_TYPE_TV && pInfo->nSeason >= 0 && pInfo->nEpisode >= 0)
	{
		SeriesSeasonData *pSeasonData = NULL;

		if (pSeriesCache)
		{
			auto it = pSeriesCache->find(pInfo->strID);
			if (it != pSeriesCache->end())
			{
				auto seasonIt = it->second.seasons.find((int)pInfo->nSeason);
				if (seasonIt != it->second.seasons.end())
					pSeasonData = &seasonIt->second;
			}

			if (!pSeasonData)
			{
				RString strSeasonURL = _T("https://www.omdbapi.com/?apikey=") + strOMDbAPIKey +
					_T("&i=") + pInfo->strID +
					_T("&Season=") + NumberToString(pInfo->nSeason) +
					_T("&r=xml");

				RXMLFile2 xmlSeasonFile;
				if (OMDbRequest(strSeasonURL, xmlSeasonFile))
				{
					if (OMDbIsRateLimited(xmlSeasonFile))
						return DBI_STATUS_RATELIMITED;

					if (OMDbIsResponseTrue(xmlSeasonFile))
					{
						SeriesSeasonData newData;
						const RXMLElem2 &seasonRoot = xmlSeasonFile.GetRootElem();
						const RArray<RXMLElem2*> &epChildren = seasonRoot.GetChildren();

						for (INT_PTR i = 0; i < epChildren.GetSize(); ++i)
						{
							if (epChildren[i]->GetName() == _T("episode"))
							{
								newData.episodeTitles.Add(epChildren[i]->GetAttribute(_T("title")));
								newData.episodeIDs.Add(epChildren[i]->GetAttribute(_T("imdbID")));
								newData.episodeRatings.Add(epChildren[i]->GetAttribute(_T("imdbRating")));
								newData.episodeVotes.Add(epChildren[i]->GetAttribute(_T("imdbVotes")));
								newData.episodeReleased.Add(epChildren[i]->GetAttribute(_T("released")));
							}
						}

						(*pSeriesCache)[pInfo->strID].seasons[(int)pInfo->nSeason] = newData;
						pSeasonData = &(*pSeriesCache)[pInfo->strID].seasons[(int)pInfo->nSeason];
					}
				}
			}
		}

		if (pSeasonData)
		{
			INT_PTR nEpIndex = pInfo->nEpisode - 1;
			if (nEpIndex >= 0 && nEpIndex < pSeasonData->episodeTitles.GetSize())
			{
				if (!pSeasonData->episodeTitles[nEpIndex].IsEmpty())
					pInfo->strEpisodeName = pSeasonData->episodeTitles[nEpIndex];

				RString strEpID = pSeasonData->episodeIDs[nEpIndex];
				if (!strEpID.IsEmpty())
					pInfo->strEpisodeID = strEpID;

				RString strEpRating = pSeasonData->episodeRatings[nEpIndex];
				if (!strEpRating.IsEmpty() && strEpRating != _T("N/A"))
					pInfo->fRating = StringToFloat(strEpRating);

				RString strEpVotes = pSeasonData->episodeVotes[nEpIndex];
				if (!strEpVotes.IsEmpty() && strEpVotes != _T("N/A"))
				{
					RString strVotes = strEpVotes;
					strVotes.Replace(_T(","), _T(""));
					pInfo->nVotes = StringToNumber(strVotes);
				}

				pInfo->fRatingMax = 10.0f;
			}
		}
	}

	RString strTitle = pMovie->GetAttribute(_T("title"));
	if (!strTitle.IsEmpty())
		pInfo->strTitle = strTitle;

	RString strYear = pMovie->GetAttribute(_T("year"));
	if (!strYear.IsEmpty() && strYear != _T("N/A"))
		pInfo->strYear = strYear;

	RString strRated = pMovie->GetAttribute(_T("rated"));
	if (!strRated.IsEmpty() && strRated != _T("N/A"))
		pInfo->strContentRating = strRated;

	RString strReleased = pMovie->GetAttribute(_T("released"));
	if (!strReleased.IsEmpty() && strReleased != _T("N/A") && pInfo->bType == DB_TYPE_TV)
		pInfo->strAirDate = strReleased;

	RString strRuntime = pMovie->GetAttribute(_T("runtime"));
	if (!strRuntime.IsEmpty() && strRuntime != _T("N/A"))
		pInfo->nRuntime = StringToNumber(StripMinFromRuntime(strRuntime));

	RString strGenre = pMovie->GetAttribute(_T("genre"));
	if (!strGenre.IsEmpty() && strGenre != _T("N/A"))
		pInfo->strGenres = CommaToPipe(strGenre);

	RString strDirector = pMovie->GetAttribute(_T("director"));
	if (!strDirector.IsEmpty() && strDirector != _T("N/A"))
		pInfo->strDirectors = CommaToPipe(strDirector);

	RString strWriter = pMovie->GetAttribute(_T("writer"));
	if (!strWriter.IsEmpty() && strWriter != _T("N/A"))
		pInfo->strWriters = CommaToPipe(strWriter);

	RString strActors = pMovie->GetAttribute(_T("actors"));
	if (!strActors.IsEmpty() && strActors != _T("N/A"))
		pInfo->strStars = CommaToPipe(strActors);

	RString strPlot = pMovie->GetAttribute(_T("plot"));
	if (!strPlot.IsEmpty() && strPlot != _T("N/A"))
		pInfo->strStoryline = strPlot;

	RString strCountry = pMovie->GetAttribute(_T("country"));
	if (!strCountry.IsEmpty() && strCountry != _T("N/A"))
		pInfo->strCountries = CommaToPipe(strCountry);

	RString strPoster = pMovie->GetAttribute(_T("poster"));
	if (!strPoster.IsEmpty() && strPoster != _T("N/A"))
	{
		if (_tcsicmp(GETPREFSTR(_T("InfoService"), _T("Poster")), _T("imdb.com")) == 0)
			URLToData(strPoster, pInfo->posterData);
	}

	RString strImdbRating = pMovie->GetAttribute(_T("imdbRating"));
	if (!strImdbRating.IsEmpty() && strImdbRating != _T("N/A"))
	{
		pInfo->fRating = StringToFloat(strImdbRating);
		pInfo->fIMDbRating = pInfo->fRating;
	}

	pInfo->fRatingMax = 10.0f;
	pInfo->fIMDbRatingMax = 10.0f;

	RString strImdbVotes = pMovie->GetAttribute(_T("imdbVotes"));
	if (!strImdbVotes.IsEmpty() && strImdbVotes != _T("N/A"))
	{
		RString strVotes = strImdbVotes;
		strVotes.Replace(_T(","), _T(""));
		pInfo->nVotes = StringToNumber(strVotes);
		pInfo->nIMDbVotes = pInfo->nVotes;
	}

	RString strMetascore = pMovie->GetAttribute(_T("metascore"));
	if (!strMetascore.IsEmpty() && strMetascore != _T("N/A"))
		pInfo->nMetascore = StringToNumber(strMetascore);

	if (pInfo->strTitle.GetLength() >= 4 && pInfo->strTitle.Left(4) == _T("The "))
		pInfo->strTitle = pInfo->strTitle.Mid(4) + _T(", The");
	else if (pInfo->strTitle.GetLength() >= 2 && pInfo->strTitle.Left(2) == _T("A "))
		pInfo->strTitle = pInfo->strTitle.Mid(2) + _T(", A");

	return DBI_STATUS_UPDATED;
}
