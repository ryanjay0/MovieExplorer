#pragma once

class Resume
{
	public:

		PROCESS_INFORMATION processInfo;
		STARTUPINFO info;

		Resume();
		void ReadVlcResumeFile();
		void LaunchVlc(RString strFileName, UINT64 resumeTime);
};
