#pragma once

class Resume
{
	public:

		PROCESS_INFORMATION processInfo;
		STARTUPINFO info;

		Resume();
		void ReadVlcResumeFile();
		void UpdateResumeTimes();
		void LaunchVlc(RString strFileName, UINT64 resumeTime);
		void WaitForExitAndRead();
};
