#include "process/process.h"
#include "process/win32process.h"
#include "ProcessSpawner.h"
#include <tlhelp32.h>

CProcess* ProcessSpawner::GetByWindowName(const char* winName) {
	HWND hwnd = ::FindWindow(0, winName);
	if (!hwnd) {
		return nullptr;  // Return nullptr to indicate failure
	}

	DWORD pid;
	::GetWindowThreadProcessId(hwnd, &pid);
	if (!pid)
		return nullptr;  // Return nullptr to indicate failure

	return ProcessSpawner::Get(pid);  // Return a valid CProcess* object
}

CProcess* ProcessSpawner::Get(const char* processName) {
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	// Take a snapshot of all processes in the system.
	hProcessSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE)
		return NULL;

	// Set the size of the structure before using it.
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!::Process32First(hProcessSnap, &pe32))
		goto error;

	do
	{
		if (_stricmp(processName, pe32.szExeFile) == 0)
		{
			::CloseHandle(hProcessSnap);
			return ProcessSpawner::Get(pe32.th32ProcessID);
		}

	} while (::Process32Next(hProcessSnap, &pe32));


error:
	::CloseHandle(hProcessSnap);
	return NULL;
}

CProcess* ProcessSpawner::Get(unsigned int processId) {
	HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if (!hProcess)
		return NULL;

	CProcess* process = new Win32Process(hProcess);
	if (!process) {
		::CloseHandle(hProcess);
		return NULL;
	}

	return process;
}

CProcess* ProcessSpawner::Create(const char* appFileName, CThread::ThreadState initalState, const char* initialWorkpath, CProcess::CreationError* pErr) {
	CProcess* process;
	DWORD _flags;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(STARTUPINFO));
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	if (initalState == CThread::NORMAL)
		_flags = 0;
	else if (initalState == CThread::SUSPENDED)
		_flags = CREATE_SUSPENDED;

	int res = ::CreateProcess(NULL, (char*)appFileName, NULL, NULL, FALSE, _flags, NULL, initialWorkpath, &si, &pi);
	if (res == 0) {
		if (pErr) {
			int err = GetLastError();
			if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND || err == ERROR_DIRECTORY) {
				*pErr = CProcess::FILE_NOT_FOUND;
			}
			else if (err == ERROR_BAD_EXE_FORMAT) {
				*pErr = CProcess::BAD_SYSTEM_EXECUTABLE_FILE;
			}
		}
		return NULL;
	}

	process = new Win32Process(pi);
	if (!process) {
		if (pErr)
			*pErr = CProcess::SYSTEM_ERROR;
		::CloseHandle(pi.hThread);
		::CloseHandle(pi.hProcess);
		return NULL;
	}

	if (pErr)
		*pErr = CProcess::SUCCESS;

	return process;
}

bool ProcessSpawner::GetProcessList(std::vector<ProcessBasicInfo>& processList) {
	bool error;
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	// Take a snapshot of all processes in the system.
	hProcessSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hProcessSnap == INVALID_HANDLE_VALUE)
		return false;

	error = true;
	// Set the size of the structure before using it.
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!::Process32First(hProcessSnap, &pe32))
		goto _finish;

	do
	{
		ProcessBasicInfo info;
		info.id = pe32.th32ProcessID;
		::strcpy(info.name, pe32.szExeFile);
		processList.push_back(info);

	} while (::Process32Next(hProcessSnap, &pe32));

	error = false;

_finish:
	::CloseHandle(hProcessSnap);
	return false;
}

