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

	RString strFilePath = _T("C:\\Users\\");
	strFilePath += username;
	strFilePath += _T("\\AppData\\Roaming\\vlc\\vlc-qt-interface.ini");

	RString strVlcFile;
	if (!FileToString(strFilePath, strVlcFile))
		return;

	RArray<RString> moviesArr;
	RArray<int> timesArr;

	INT_PTR nPos = strVlcFile.Find(_T("list="));
	if (nPos >= 0)
	{
		nPos += 5;
		INT_PTR nLineEnd = strVlcFile.Find(_T('\n'), nPos);
		if (nLineEnd < 0) nLineEnd = strVlcFile.GetLength();
		RString strList = strVlcFile.Mid(nPos, nLineEnd - nPos);
		strList.Trim();
		strList.Replace(_T(" "), _T(""));

		RArray<const TCHAR*> moviesTemp = SplitString(strList, _T(","), true);
		for (int i = 0; i < moviesTemp.GetSize(); i++)
		{
			RString strTempMovie = URLDecode(moviesTemp[i]);

			if (strTempMovie.Left(8) == _T("file:///"))
				strTempMovie = strTempMovie.Right(strTempMovie.GetLength() - 8);
			else if (strTempMovie.Left(7) == _T("file://"))
				strTempMovie = strTempMovie.Right(strTempMovie.GetLength() - 7);

			strTempMovie = CorrectPath(strTempMovie);
			moviesArr.Add(strTempMovie);
		}
	}

	nPos = strVlcFile.Find(_T("times="));
	if (nPos >= 0)
	{
		nPos += 6;
		INT_PTR nLineEnd = strVlcFile.Find(_T('\n'), nPos);
		if (nLineEnd < 0) nLineEnd = strVlcFile.GetLength();
		RString strTimes = strVlcFile.Mid(nPos, nLineEnd - nPos);
		strTimes.Trim();
		strTimes.Replace(_T(" "), _T(""));

		RArray<const TCHAR*> strTimesArr = SplitString(strTimes, _T(","), true);
		for (int i = 0; i < strTimesArr.GetSize(); i++)
			timesArr.Add((int)(StringToNumber(strTimesArr[i]) / 1000));
	}

	for (int i = 0; i < moviesArr.GetSize() && i < timesArr.GetSize(); i++)
		GetDB()->UpdateResumeTime(moviesArr[i], timesArr[i]);
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
