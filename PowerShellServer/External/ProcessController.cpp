// ProcessController.cpp : Defines the functions for the static library.
//
#include <Windows.h>
#include "ProcessController.h"


namespace ProcessController
{
	inline decltype(&CreateProcessA) _CreateProcessA;
	inline decltype(&CreatePipe) _CreatePipe;
	inline decltype(&SetHandleInformation) _SetHandleInformation;
	inline decltype(&CloseHandle) _CloseHandle;
	inline decltype(&WriteFile) _WriteFile;
	inline decltype(&ReadFile) _ReadFile;

	void INIT_ProcessController(
		void* pCreateProcess,
		void* pCreatePipe,
		void* pSetHandleInformation,
		void* pCloseHandle,
		void* pWriteFile,
		void* pReadFile
	)
	{
		_CreateProcessA = (decltype(&CreateProcessA))(pCreateProcess);
		_CreatePipe = (decltype(&CreatePipe))(pCreatePipe);
		_SetHandleInformation = (decltype(&SetHandleInformation))(pSetHandleInformation);
		_CloseHandle = (decltype(&CloseHandle))(pCloseHandle);
		_WriteFile = (decltype(&WriteFile))(pWriteFile);
		_ReadFile = (decltype(&ReadFile))(pReadFile);
	}

	CProcess::CProcess()
	{
		// Create a pipe for the child process's STDOUT. 
		SECURITY_ATTRIBUTES saAttr;
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		if (!_CreatePipe(&m_hSTDOUTRead, &m_hSTDOUTWrite, &saAttr, 0))
			return;

		if (!_SetHandleInformation(m_hSTDOUTRead, HANDLE_FLAG_INHERIT, 0))
			return;

		if (!_CreatePipe(&m_hSTDINRead, &m_hSTDINWrite, &saAttr, 0))
			return;

		if (!_SetHandleInformation(m_hSTDINWrite, HANDLE_FLAG_INHERIT, 0))
			return;

		m_bFailedToInitialize = false;
	}
	CProcess::~CProcess()
	{
		if (!m_bClosedProcess)
			CloseProcess();
	}
	bool CProcess::CloseProcess()
	{
		if (!m_bFailedToStartProcess && !m_bFailedToInitialize)
		{
			_CloseHandle(m_hProcess);
			_CloseHandle(m_hThread);
		}

		if (!m_bFailedToInitialize)
		{
			_CloseHandle(m_hSTDINWrite);
			_CloseHandle(m_hSTDOUTRead);
		}

		m_bClosedProcess = true;

		return true;
	}


	bool CProcess::StartProcess(const char* szExecutablePath, char* szCmdLine, bool bHiddenWindow)
	{
		if (m_bFailedToInitialize)
			return false;

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		if (bHiddenWindow)
		{
			si.dwFlags |= STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
		}

		si.dwFlags |= STARTF_USESTDHANDLES;

		si.hStdError = m_hSTDOUTWrite;
		si.hStdOutput = m_hSTDOUTWrite;
		si.hStdInput = m_hSTDINRead;

		BOOL bRet = _CreateProcessA(szExecutablePath,
			szCmdLine,
			NULL,
			NULL,
			TRUE, // Inherit handles for stdout and stdin redirecting
			NORMAL_PRIORITY_CLASS | (bHiddenWindow ? CREATE_NO_WINDOW : NULL),
			NULL,
			NULL,
			&si,
			&pi
		);

		if (!bRet)
			return false;

		m_bFailedToStartProcess = false;

		m_hProcess = pi.hProcess;
		m_hThread = pi.hThread;

		_CloseHandle(m_hSTDOUTWrite);
		_CloseHandle(m_hSTDINRead);

		return true;
	}



	bool CProcess::SendDataToProcess(
		void* pData,
		size_t nDataSize
	){

		if (m_bFailedToInitialize || m_bFailedToStartProcess)
			return false;

		DWORD dwWritten = 0;
		BOOL bSuccess = FALSE;
		while (dwWritten < nDataSize)
		{
			
			bSuccess = _WriteFile(m_hSTDINWrite, pData, nDataSize, &dwWritten, NULL);
			
			if (!bSuccess)
				return false;
		}

		return true;
	}

	// Need to do overlapped i/o eventually
	// https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports
	bool CProcess::RecvDataFromProcess(void* pData, size_t nBufferSize, size_t* pnDataRead)
	{
		*pnDataRead = 0;
		if (m_bFailedToInitialize || m_bFailedToStartProcess)
			return false;

		DWORD dwBytesAvail = 0;

		if (!PeekNamedPipe(m_hSTDOUTRead, NULL, 0, NULL, &dwBytesAvail, NULL))
			return false;
		
		if(dwBytesAvail)
			return ReadFile(m_hSTDOUTRead, pData, nBufferSize, (LPDWORD)pnDataRead, NULL);

		return false;
	}


	bool CProcess::RedirectHandles()
	{
		if (!SetStdHandle(STD_INPUT_HANDLE, m_hSTDINRead))
			return false;

		if (!SetStdHandle(STD_OUTPUT_HANDLE, m_hSTDOUTWrite))
			return false;

		if (!SetStdHandle(STD_ERROR_HANDLE, m_hSTDOUTWrite))
			return false;

		return true;
	}

}