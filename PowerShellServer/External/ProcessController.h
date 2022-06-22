#pragma once


namespace ProcessController
{

	void INIT_ProcessController(
		void* CreateProcessA,
		void* CreatePipe,
		void* SetHandleInformation,
		void* CloseHandle,
		void* WriteFile,
		void* ReadFile
	);

	class CProcess
	{
	public:
		CProcess();
		~CProcess();

		bool StartProcess(
			const char* szExecutablePath,
			char* szCmdLine,
			bool bHiddenWindow = true
		);

		bool CloseProcess();

		// if manually set process handle to local process
		bool RedirectHandles();

		inline const void* GetProcessHandle() { return m_hProcess; }
		inline const void* GetThreadHandle() { return m_hThread; }
		inline bool SetProcessHandle(void* hHandle, void* hThreadHandle) 
		{ 
			if (m_bFailedToInitialize)
				return false;

			m_hProcess = hHandle; 
			m_bFailedToStartProcess = false;
			m_hThread = hThreadHandle;
		}

		// Through stdout and stdin
		bool SendDataToProcess(void* pData, size_t nDataSize);
		bool RecvDataFromProcess(void* pData, size_t nBufferSize, size_t* pnDataRead);

	private:
		void* m_hSTDINRead;
		void* m_hSTDINWrite;
		void* m_hSTDOUTRead;
		void* m_hSTDOUTWrite;

		void* m_hProcess;
		void* m_hThread;

		bool m_bFailedToInitialize = true;
		bool m_bFailedToStartProcess = true;
		bool m_bClosedProcess = false;
	};


}