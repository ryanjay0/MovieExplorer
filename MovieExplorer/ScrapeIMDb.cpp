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

DWORD ScrapeIMDb(DBINFO *pInfo, RString strOMDbAPIKey)
{
	if (!pInfo || pInfo->strSearchTitle.IsEmpty() || pInfo->strServiceName != _T("imdb.com"))
		{ASSERT(false); return DBI_STATUS_SCRAPEERROR;}

	if (strOMDbAPIKey.IsEmpty())
		return DBI_STATUS_CONNERROR;

	// Find movie ID when it is not provided

	if (pInfo->strID.IsEmpty())
	{
		RString strURL = _T("https://www.omdbapi.com/?apikey=") + strOMDbAPIKey +
			_T("&s=") + URLEncode(pInfo->strSearchTitle) +
			_T("&r=xml");

		if (!pInfo->strSearchYear.IsEmpty() && pInfo->bType != DB_TYPE_TV)
			strURL += _T("&y=") + pInfo->strSearchYear;

		if (pInfo->bType == DB_TYPE_TV)
			strURL += _T("&type=series");
		else if (pInfo->bType == DB_TYPE_MOVIE)
			strURL += _T("&type=movie");

		RXMLFile2 xmlFile;
		if (!OMDbRequest(strURL, xmlFile))
			return DBI_STATUS_CONNERROR;

		if (!OMDbIsResponseTrue(xmlFile))
			return DBI_STATUS_UNKNOWN;

		const RXMLElem2 &root = xmlFile.GetRootElem();
		const RArray<RXMLElem2*> &children = root.GetChildren();

		RString strBestID;
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

			if (_tcsicmp(strResultTitle, pInfo->strSearchTitle) == 0)
				nScore += 100;
			else if (strResultTitle.FindNoCase(pInfo->strSearchTitle) >= 0)
				nScore += 50;

			if (!pInfo->strSearchYear.IsEmpty() && strResultYear == pInfo->strSearchYear)
				nScore += 30;

			if (pInfo->bType == DB_TYPE_TV && _tcsicmp(strResultType, _T("series")) == 0)
				nScore += 20;
			else if (pInfo->bType == DB_TYPE_MOVIE && _tcsicmp(strResultType, _T("movie")) == 0)
				nScore += 20;

			if (nScore > nBestMatchScore)
			{
				nBestMatchScore = nScore;
				strBestID = strResultID;
			}
		}

		if (strBestID.IsEmpty())
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
