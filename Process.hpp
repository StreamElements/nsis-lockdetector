#pragma once

#include <windows.h>
#include <processthreadsapi.h>
#include <psapi.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>

class Process
{
public:
	Process() {}
	Process(const DWORD id);
	Process(const Process& other) : Process(other.m_id) {}
	~Process();

	const DWORD id() const { return m_id; }
	const HANDLE handle() const { return m_handle; }
	const TCHAR* const path() const { return m_imagePath; }
	const HICON icon();
	const HWND mainWindowHandle();
	const TCHAR* const mainWindowTitle();

	bool compare(Process& other) { return m_id == other.m_id; }

	bool terminateAsync(const UINT exitCode) { return ::TerminateProcess(m_handle, exitCode); }
	bool wait(const DWORD timeoutMilliseconds = INFINITE) { return ::WaitForSingleObject(m_handle, timeoutMilliseconds) == WAIT_OBJECT_0; }
	bool terminate(const UINT exitCode, const DWORD timeoutMilliseconds = INFINITE)
	{
		if (terminateAsync(exitCode)) {
			return wait(timeoutMilliseconds);
		}

		return false;
	}
	bool running();
	DWORD exitCode();

	static const bool queryAllProcesses(std::vector<std::shared_ptr<Process>>& output, std::function<bool(Process&)> filter);
	static const bool queryAllProcesses(std::vector<std::shared_ptr<Process>>& output);
	static const bool queryAllProcesses(std::vector<std::wstring>& lockedFiles, std::vector<std::shared_ptr<Process>>& output);

private:
	DWORD m_id = 0;
	HANDLE m_handle = INVALID_HANDLE_VALUE;
	TCHAR* m_imagePath = nullptr;
	HICON m_icon = (HICON)INVALID_HANDLE_VALUE;
	TCHAR* m_mainWindowTitle = nullptr;
	HWND m_mainWindowHandle = (HWND)INVALID_HANDLE_VALUE;
};
