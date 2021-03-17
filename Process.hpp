#pragma once

#include <windows.h>
#include <processthreadsapi.h>
#include <psapi.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

class Process; // forward declaration

class ProcessListContext
{
public:
	ProcessListContext() : m_list(std::make_shared<std::vector<std::shared_ptr<Process>>>()) {}
	~ProcessListContext() {}

public:
	std::shared_ptr<std::vector<std::shared_ptr<Process>>> items() { return m_list; }

public:
	virtual bool terminateAll() = 0;

private:
	std::shared_ptr<std::vector<std::shared_ptr<Process>>> m_list;
};

class ProcessListProvider
{
public:
	ProcessListProvider() {}
	~ProcessListProvider() {}

public:
	virtual const bool queryAllProcesses(
		std::vector<std::wstring>& patterns,
		std::shared_ptr<ProcessListContext>& output) = 0;
};

class PsListProcessListProvider : public ProcessListProvider
{
public:
	PsListProcessListProvider() {}
	~PsListProcessListProvider() {}

public:
	virtual const bool queryAllProcesses(
		std::vector<std::wstring>& patterns,
		std::shared_ptr<ProcessListContext>& output) override;
};

class RestartManagerProcessListProvider : public ProcessListProvider
{
public:
	RestartManagerProcessListProvider() {}
	~RestartManagerProcessListProvider() {}

public:
	virtual const bool queryAllProcesses(
		std::vector<std::wstring>& patterns,
		std::shared_ptr<ProcessListContext>& output) override;
};

class Process
{
public:
	Process() {}
	Process(const DWORD id, const std::wstring displayName = L"");
	Process(const Process& other) : Process(other.m_id) {}
	~Process();

	const DWORD id() const { return m_id; }
	const HANDLE handle() const { return m_handle; }
	const TCHAR* const path() const { return m_imagePath; }
	const HICON icon();
	const HWND mainWindowHandle();
	const TCHAR* const mainWindowTitle();

	const TCHAR* const displayName() {
		if (m_displayName)
			return m_displayName;
		
		if (mainWindowTitle())
			return mainWindowTitle();
		
		return path();
	}

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

private:
	DWORD m_id = 0;
	HANDLE m_handle = INVALID_HANDLE_VALUE;
	TCHAR* m_imagePath = nullptr;
	HICON m_icon = (HICON)INVALID_HANDLE_VALUE;
	TCHAR* m_mainWindowTitle = nullptr;
	HWND m_mainWindowHandle = (HWND)INVALID_HANDLE_VALUE;
	TCHAR* m_displayName = nullptr;
};
