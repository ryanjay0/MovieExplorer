#include "stdafx.h"
#include "MovieExplorer.h"

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

static bool OMDbSearch(RString strAPIKey, RString strTitle, RString strYear, BYTE bType, RString &strBestID)
{
	RString strURL = OMDbBuildSearchURL(strAPIKey, strTitle, strYear, bType);

	RXMLFile2 xmlFile;
	if (!OMDbRequest(strURL, xmlFile))
		return false;

	if (!OMDbIsResponseTrue(xmlFile))
		return false;

	return OMDbPickBestResult(xmlFile, strTitle, strYear, bType, strBestID);
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

DWORD ScrapeIMDb(DBINFO *pInfo, RString strOMDbAPIKey)
{
	if (!pInfo || pInfo->strSearchTitle.IsEmpty() || pInfo->strServiceName != _T("imdb.com"))
		{ASSERT(false); return DBI_STATUS_SCRAPEERROR;}

	if (strOMDbAPIKey.IsEmpty())
		return DBI_STATUS_CONNERROR;

	// Find movie ID when it is not provided

	if (pInfo->strID.IsEmpty())
	{
		RString strBestID;

		// Attempt 1: search with title + year

		bool bFound = OMDbSearch(strOMDbAPIKey, pInfo->strSearchTitle, pInfo->strSearchYear, pInfo->bType, strBestID);

		// Attempt 2: retry without year (year in filename may not match OMDb)

		if (!bFound && !pInfo->strSearchYear.IsEmpty() && pInfo->bType != DB_TYPE_TV)
			bFound = OMDbSearch(strOMDbAPIKey, pInfo->strSearchTitle, RString(), pInfo->bType, strBestID);

		// Attempt 3: retry with apostrophe variants (filenames often strip apostrophes)

		if (!bFound)
		{
			RString strApostrophe = TryApostropheVariants(pInfo->strSearchTitle);
			if (strApostrophe != pInfo->strSearchTitle)
				bFound = OMDbSearch(strOMDbAPIKey, strApostrophe, pInfo->strSearchYear, pInfo->bType, strBestID);

			if (!bFound && !pInfo->strSearchYear.IsEmpty() && pInfo->bType != DB_TYPE_TV)
				bFound = OMDbSearch(strOMDbAPIKey, strApostrophe, RString(), pInfo->bType, strBestID);
		}

		// Attempt 4: retry without country suffix (e.g. "Love Island US" -> "Love Island")

		if (!bFound && pInfo->bType == DB_TYPE_TV)
		{
			RString strStripped = TryStripCountrySuffix(pInfo->strSearchTitle);
			if (strStripped != pInfo->strSearchTitle)
			{
				bFound = OMDbSearch(strOMDbAPIKey, strStripped, pInfo->strSearchYear, pInfo->bType, strBestID);

				if (!bFound && !pInfo->strSearchYear.IsEmpty())
					bFound = OMDbSearch(strOMDbAPIKey, strStripped, RString(), pInfo->bType, strBestID);
			}
		}

		if (!bFound)
			return DBI_STATUS_UNKNOWN;

		pInfo->strID = strBestID;
	}

	// Retrieve movie data

	RString strURL = _T("https://www.omdbapi.com/?apikey=") + strOMDbAPIKey +
		_T("&i=") + pInfo->strID +
		_T("&plot=full&r=xml");

	RXMLFile2 xmlFile;
	if (!OMDbRequest(strURL, xmlFile))
		return DBI_STATUS_CONNERROR;

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

	// Extract type

	RString strType = pMovie->GetAttribute(_T("type"));
	if (_tcsicmp(strType, _T("series")) == 0)
		pInfo->bType = DB_TYPE_TV;
	else if (_tcsicmp(strType, _T("episode")) == 0)
		pInfo->bType = DB_TYPE_TV;
	else
		pInfo->bType = DB_TYPE_MOVIE;

	// For TV episodes with season/episode info, fetch the specific episode

	if (pInfo->bType == DB_TYPE_TV && pInfo->nSeason >= 0 && pInfo->nEpisode >= 0)
	{
		RString strEpURL = _T("https://www.omdbapi.com/?apikey=") + strOMDbAPIKey +
			_T("&i=") + pInfo->strID +
			_T("&Season=") + NumberToString(pInfo->nSeason) +
			_T("&Episode=") + NumberToString(pInfo->nEpisode) +
			_T("&r=xml");

		RXMLFile2 xmlEpFile;
		if (OMDbRequest(strEpURL, xmlEpFile) && OMDbIsResponseTrue(xmlEpFile))
		{
			const RXMLElem2 &epRoot = xmlEpFile.GetRootElem();
			const RArray<RXMLElem2*> &epChildren = epRoot.GetChildren();

			for (INT_PTR i = 0; i < epChildren.GetSize(); ++i)
			{
				if (epChildren[i]->GetName() == _T("episode"))
				{
					RString strEpTitle = epChildren[i]->GetAttribute(_T("title"));
					RString strEpID = epChildren[i]->GetAttribute(_T("imdbID"));
					RString strEpRating = epChildren[i]->GetAttribute(_T("imdbRating"));
					RString strEpVotes = epChildren[i]->GetAttribute(_T("imdbVotes"));

					if (!strEpTitle.IsEmpty())
						pInfo->strEpisodeName = strEpTitle;

					if (!strEpRating.IsEmpty() && strEpRating != _T("N/A"))
						pInfo->fRating = StringToFloat(strEpRating);

					if (!strEpVotes.IsEmpty() && strEpVotes != _T("N/A"))
					{
						RString strVotes = strEpVotes;
						strVotes.Replace(_T(","), _T(""));
						pInfo->nVotes = StringToNumber(strVotes);
					}

					pInfo->fRatingMax = 10.0f;

					if (!strEpID.IsEmpty())
					{
						pInfo->strID = strEpID;
					}

					break;
				}
			}
		}
	}

	// Extract basic fields from the movie element

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

	// Extract poster

	RString strPoster = pMovie->GetAttribute(_T("poster"));
	if (!strPoster.IsEmpty() && strPoster != _T("N/A"))
	{
		if (_tcsicmp(GETPREFSTR(_T("InfoService"), _T("Poster")), _T("imdb.com")) == 0)
			URLToData(strPoster, pInfo->posterData);
	}

	// Extract ratings

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

	// Title formatting: move "The" and "A" to end

	if (pInfo->strTitle.GetLength() >= 4 && pInfo->strTitle.Left(4) == _T("The "))
		pInfo->strTitle = pInfo->strTitle.Mid(4) + _T(", The");
	else if (pInfo->strTitle.GetLength() >= 2 && pInfo->strTitle.Left(2) == _T("A "))
		pInfo->strTitle = pInfo->strTitle.Mid(2) + _T(", A");

	return DBI_STATUS_UPDATED;
}
