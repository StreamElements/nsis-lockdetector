#include "stdafx.h"
#include "Process.hpp"


Process::Process(const DWORD id) :
	m_id(id),
	m_handle(0),
	m_icon(0),
	m_imagePath(0)
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
		for (int i = 0; i < bufLen / sizeof(DWORD); ++i) {
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
