#include "stdafx.h"
#include "Process.hpp"

#include <RestartManager.h>
#include <string>
#include <vector>
#include <set>
#include <filesystem>

// Convert a wide Unicode string to an UTF8 string
static std::string utf8_encode(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

// Convert an UTF8 string to a wide Unicode String
static std::wstring utf8_decode(const std::string& str)
{
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

// Function: Search using wildcards.
//
// http://xoomer.virgilio.it/acantato/dev/wildcard/wildmatch.html#evolution
//
static BOOL wildcmp(const wchar_t* pat, const wchar_t* str) {
	wchar_t* s;
	wchar_t* p;
	BOOL star = FALSE;

loopStart:
	for (s = const_cast<wchar_t*>(str), p = const_cast<wchar_t*>(pat); *s; ++s, ++p) {
		switch (*p) {
		case L'?':
			if (*s == L'.') goto starCheck;
			break;
		case L'*':
			star = TRUE;
			str = s, pat = p;
			do { ++pat; } while (*pat == L'*');
			if (!*pat) return TRUE;
			goto loopStart;
		default:
			// if (_totupper(*s) != _totupper(*p))
			if (towupper(*s) != towupper(*p))
				goto starCheck;
			break;
		} /* endswitch */
	} /* endfor */
	while (*p == L'*') ++p;
	return (!*p);

starCheck:
	if (!star) return FALSE;
	str++;
	goto loopStart;
}


static bool wildmatch(std::vector<std::wstring>& patterns, const TCHAR* path)
{
	if (!path) {
		return false;
	}

#ifdef UNICODE
	std::wstring wpath = path;
#else
	std::string ansiPath(path);
	std::wstring wpath(ansiPath.begin(), ansiPath.end());
#endif
	for (auto pattern : patterns) {
		if (wildcmp(pattern.c_str(), wpath.c_str())) {
			return true;
		}
	}

	return false;
}

static void GetFilesByWildcard(std::wstring rootPath, std::wstring wildcard, std::vector<std::wstring>& output)
{
	for (auto& entry :
		std::filesystem::recursive_directory_iterator(
			rootPath)) {

		if (!entry.is_regular_file())
			continue;

		if (wildcmp(wildcard.c_str(), entry.path().wstring().c_str())) {
			output.push_back(entry.path().wstring());
		}
	}
}

Process::Process(const DWORD id, const std::wstring displayName) :
	m_id(id),
	m_handle(0),
	m_icon(0),
	m_imagePath(0),
	m_mainWindowTitle(0),
	m_mainWindowHandle(0),
	m_displayName(0)
{
	m_handle = OpenProcess(
		PROCESS_TERMINATE | 
		PROCESS_QUERY_INFORMATION |
		SYNCHRONIZE,
		TRUE,
		m_id);

	if (m_handle) {
		DWORD bufLen = 32768;
		TCHAR* buf = new TCHAR[bufLen];

		if (0 != QueryFullProcessImageName(
			m_handle,
			0,
			buf,
			&bufLen)) {
			buf[bufLen] = 0;

			m_imagePath = new TCHAR[bufLen + 1];
			memcpy(m_imagePath, buf, (bufLen + 1) * sizeof(TCHAR));
		}

		delete[] buf;
	}

#ifdef _UNICODE
	if (displayName.size())
		m_displayName = wcsdup(displayName.c_str());
#else
	m_displayName = _strdup(utf8_encode(displayName).c_str());
#endif
}

const HICON Process::icon()
{
	if (!m_icon) {
		m_icon = ExtractIcon(NULL, path(), 0);
	}

	return m_icon;
}

Process::~Process()
{
	if (m_handle) {
		CloseHandle(m_handle);
	}

	if (m_imagePath) {
		delete[] m_imagePath;
	}

	if (m_mainWindowTitle) {
		delete[] m_mainWindowTitle;
	}

	if (m_displayName) {
		delete[] m_displayName;
	}

	if (m_icon) {
		DestroyIcon(m_icon);
	}
}

bool Process::running()
{
	DWORD exitCode;
	if (GetExitCodeProcess(m_handle, &exitCode)) {
		return (exitCode == STILL_ACTIVE);
	}

	return false;
}

DWORD Process::exitCode()
{
	DWORD exitCode;
	if (GetExitCodeProcess(m_handle, &exitCode)) {
		return exitCode;
	}

	return 0xffffffffL;
}

const HWND Process::mainWindowHandle()
{
	if (!m_mainWindowHandle) {
		std::vector <HWND> vWindows;

		HWND hwnd = NULL;

		do
		{
			hwnd = FindWindowEx(GetDesktopWindow(), hwnd, NULL, NULL);

			if (IsWindowVisible(hwnd)) {
				DWORD dwPID = 0;
				GetWindowThreadProcessId(hwnd, &dwPID);
				if (dwPID == id()) {
					vWindows.push_back(hwnd);
				}
			}
		} while (hwnd != NULL);

		if (vWindows.size() > 0) {
			m_mainWindowHandle = vWindows[0];
		}
	}

	return m_mainWindowHandle;
}

const TCHAR* const Process::mainWindowTitle()
{
	if (!m_mainWindowTitle) {
		HWND hwnd = mainWindowHandle();

		if (hwnd) {
			const size_t increment = 32;
			size_t bufLen = 256;
			TCHAR* buf = new TCHAR[bufLen];

			size_t textLen = GetWindowText(hwnd, buf, (int)bufLen);
			while (textLen >= bufLen - 1) {
				delete[] buf;
				bufLen += increment;
				buf = new TCHAR[bufLen];

				textLen = GetWindowText(hwnd, buf, (int)bufLen);
			}

			if (textLen > 0) {
				m_mainWindowTitle = buf;
			}
			else {
				delete[] buf;
			}
		}
	}

	return m_mainWindowTitle;
}

class PsListProcessListContext : public ProcessListContext
{
public:
	PsListProcessListContext() : ProcessListContext() {}

	virtual bool terminateAll() override
	{
		bool result = true;

		for (auto& proc : *items()) {
			if (!proc->terminate(-1))
				result = false;
		}

		return result;
	}
};

const bool PsListProcessListProvider::queryAllProcesses(
	std::vector<std::wstring>& patterns,
	std::shared_ptr<ProcessListContext>& output)
{
	bool result = false;

	output = std::make_shared<PsListProcessListContext>();

	const DWORD increment = 1024;
	DWORD bufLen = sizeof(DWORD) * increment;
	DWORD bufNeeded = 0;
	DWORD* buf = new DWORD[bufLen / sizeof(DWORD)];

	while (EnumProcesses(buf, bufLen, &bufNeeded)) {
		if (bufNeeded < bufLen) {
			result = true;

			break;
		}
		else {
			delete[] buf;
			bufLen += (sizeof(DWORD) * increment);
			buf = new DWORD[bufLen / sizeof(DWORD)];
		}
	}

	if (result) {
		for (size_t i = 0; i < bufLen / sizeof(DWORD); ++i) {
			std::shared_ptr<Process> p = std::make_shared<Process>(buf[i]);

			if (wildmatch(patterns, p->path())) {
				output->items()->emplace_back(p);
			}
		}
	}

	delete[] buf;

	return result;
}

#pragma comment(lib, "Rstrtmgr.lib")

class RestartManagerProcessListContext : public ProcessListContext
{
public:
	RestartManagerProcessListContext(DWORD dwSession)
		: ProcessListContext(), m_dwSession(dwSession)
	{
	}

	~RestartManagerProcessListContext()
	{
		if (m_terminateCalled) {
			RmRestart(m_dwSession, 0, [](UINT nPercentComplete) {
				printf("RmRestart: %d %%\n", nPercentComplete);
			});
		}

		RmEndSession(m_dwSession);
	}

	virtual bool terminateAll() override
	{
		m_terminateCalled = true;

		bool result = false;

		if (ERROR_SUCCESS == RmShutdown(m_dwSession, RmForceShutdown, [](UINT nPercentComplete) {
			printf("RmShutdown: %d %%\n", nPercentComplete);
			})) {
			result = true;
		}

		return result;
	}

private:
	DWORD m_dwSession = 0L;
	bool m_terminateCalled = false;
};

const bool RestartManagerProcessListProvider::queryAllProcesses(
	std::vector<std::wstring>& patterns,
	std::shared_ptr<ProcessListContext>& output)
{
	bool returnVal = false;

	DWORD dwSession;
	WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
	DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);

	if (dwError != ERROR_SUCCESS)
		return false;

	std::vector<std::wstring> lockedFiles;

	for (auto& pattern : patterns) {
		auto full = std::filesystem::absolute(pattern);
		std::wstring filename = full.filename().wstring();
		std::wstring parentDir = full.parent_path().wstring();

		GetFilesByWildcard(parentDir, filename, lockedFiles);
	}

	std::vector<LPCWSTR> filesArray;
	for (auto& i : lockedFiles) {
		filesArray.push_back(i.c_str());
	}

	dwError = RmRegisterResources(dwSession, (UINT)filesArray.size(), filesArray.data(),
		0, NULL, 0, NULL);

	if (dwError == ERROR_SUCCESS) {
		output = std::make_shared<RestartManagerProcessListContext>(dwSession);

		DWORD dwReason;
		UINT nProcInfoNeeded = 0;
		UINT nProcInfo = 0;

		dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, nullptr, &dwReason);

		if (dwError == ERROR_SUCCESS) {
			returnVal = true;
		}
		else if (dwError == ERROR_MORE_DATA) {
			nProcInfo = nProcInfoNeeded;

			std::vector<RM_PROCESS_INFO> result;
			result.resize(nProcInfo);

			dwError = RmGetList(dwSession, &nProcInfoNeeded,
				&nProcInfo, result.data(), &dwReason);

			returnVal = dwError == ERROR_SUCCESS;

			std::set<DWORD> seenProcessIds;

			for (auto& proc : result) {
				if (seenProcessIds.count(proc.Process.dwProcessId))
					continue;

				seenProcessIds.emplace(proc.Process.dwProcessId);

				std::shared_ptr<Process> pi =
					std::make_shared<Process>(
						proc.Process.dwProcessId,
						proc.strAppName);

				output->items()->push_back(pi);
			}
		}
	}

	return returnVal;
}
