#include "stdafx.h"
#include "MovieExplorer.h"
#include "Resume.h"
#include <Windows.h>
#include <lmcons.h>

Resume::Resume()
{
	ZeroMemory(&processInfo, sizeof(processInfo));
	info = { sizeof(info) };
}

void Resume::ReadVlcResumeFile()
{
	TCHAR username[UNLEN + 1];
	DWORD username_len = UNLEN + 1;
	GetUserName(username, &username_len);

	RString strTemp;
	RString strVlcFile;

	RString strFilePath = _T("C:\\Users\\");
	strFilePath += username;
	strFilePath += _T("\\AppData\\Roaming\\vlc\\vlc-qt-interface.ini");

	if (!FileToString(strFilePath, strVlcFile))
		return;

	RArray<RString> moviesArr;
	RArray<int> timesArr;

	if (GetFirstMatch(strVlcFile, _T("list=([^$]*?$)"), &strTemp))
	{
		strTemp.Replace(_T(" "), _T(""));
		RArray<const TCHAR*> moviesTemp = SplitString(strTemp, _T(","), true);

		for(int i = 0; i < moviesTemp.GetSize(); i++)
		{
			RString strTempMovie = URLDecode(moviesTemp[i]);

			if (strTempMovie.Left(8) == _T("file:///"))
				strTempMovie = strTempMovie.Right(strTempMovie.GetLength() - 8);
			else if (strTempMovie.Left(7) == _T("file://"))
				strTempMovie = strTempMovie.Right(strTempMovie.GetLength() - 7);

			strTempMovie.Replace(_T("/"), _T("\\\\"));
			moviesArr.Add(strTempMovie);
		}
	}

	if (GetFirstMatch(strVlcFile, _T("times=([^$]*?$)"), &strTemp))
	{
		strTemp.Replace(_T(" "), _T(""));
		RArray<const TCHAR*> strTimes = SplitString(strTemp, _T(","), true);
		for(int i = 0; i < strTimes.GetSize(); i++)
			timesArr.Add((int)(StringToNumber(strTimes[i]) / 1000));
	}

	for (int i = 0; i < moviesArr.GetSize() && i < timesArr.GetSize(); i++)
	{
		RString tempMovieStr = moviesArr[i];
		tempMovieStr.Replace(_T("\\\\"), _T("\\"));
		GetDB()->UpdateResumeTime(tempMovieStr, timesArr[i]);
	}
}

void Resume::LaunchVlc(RString strFilePath, UINT64 resumeTime)
{
	if (processInfo.hProcess)
	{
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		processInfo.hProcess = NULL;
		processInfo.hThread = NULL;
	}

	RString path = GETPREFSTR(_T("Resume"), _T("VlcPath"));
	RString cmd = path + _T(" \"") + strFilePath + _T("\" --play-and-exit");
	if (resumeTime > 0)
		cmd += _T(" --start-time=") + NumberToString((INT64)resumeTime);

	TCHAR* param = new TCHAR[cmd.GetLength() + 1];
	_tcscpy(param, cmd);

	CreateProcess(NULL, param, NULL, NULL, FALSE, 0, NULL, NULL, &info, &processInfo);
	delete[] param;
}
