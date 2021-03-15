#include "stdafx.h"
#include "Process.hpp"

#include <RestartManager.h>
#include <string>
#include <vector>
#include <set>

Process::Process(const DWORD id) :
	m_id(id),
	m_handle(0),
	m_icon(0),
	m_imagePath(0),
	m_mainWindowTitle(0),
	m_mainWindowHandle(0)
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

const bool Process::queryAllProcesses(std::vector<std::shared_ptr<Process>>& output, std::function<bool(Process&)> filter)
{
	bool result = false;

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

			if (filter(*p)) {
				output.emplace_back(p);
			}
		}
	}

	delete[] buf;

	return result;
}

const bool Process::queryAllProcesses(std::vector<std::shared_ptr<Process>>& output)
{
	return queryAllProcesses(
		output,
		[](Process&) -> bool { return true; }
	);
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

#pragma comment(lib, "Rstrtmgr.lib")
const bool Process::queryAllProcesses(std::vector<std::wstring>& lockedFiles, std::vector<std::shared_ptr<Process>>& output)
{
	bool returnVal = false;

	DWORD dwSession;
	WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
	DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);

	if (dwError != ERROR_SUCCESS)
		return false;

	std::vector<LPCWSTR> filesArray;
	for (auto& i : lockedFiles) {
		filesArray.push_back(i.c_str());
	}

	dwError = RmRegisterResources(dwSession, (UINT)filesArray.size(), filesArray.data(),
		0, NULL, 0, NULL);

	if (dwError == ERROR_SUCCESS) {
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

				std::shared_ptr<Process> pi = std::make_shared<Process>(proc.Process.dwProcessId);

				output.push_back(pi);
			}
		}
	}

	RmEndSession(dwSession);

	return returnVal;
}
