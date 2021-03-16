#include "stdafx.h"
#include "ProcessList.hpp"
#include <filesystem>
#include <windows.h>

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

ProcessList::ProcessList(ProcessListMode mode) :
	m_dirty(false),
	m_running(true),
	m_mode(mode)
{
	update();

	m_thread = std::thread(thread, this);
	m_thread.detach();
}

ProcessList::~ProcessList()
{
	m_running = false;

	{
		std::unique_lock<std::mutex> lock(m_event_mutex);
		m_event.notify_one();
	}

	/*
	if (m_thread.joinable()) {
		m_thread.join();
	}*/

	{
		std::unique_lock<std::mutex> lock(m_exit_event_mutex);
		m_exit_event.wait(lock);
	}
}

void ProcessList::addPattern(const TCHAR* pattern)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

#ifdef UNICODE
	m_patternList.emplace_back(std::wstring(pattern));
#else
	std::string s(pattern);
	m_patternList.emplace_back(std::wstring(s.begin(), s.end()));
#endif

	update();
}

void ProcessList::addPatterns(const std::vector<std::wstring>& patterns)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto& pattern : patterns) {
		m_patternList.emplace_back(pattern);
	}

	update();
}

bool ProcessList::match(const TCHAR* path)
{
	if (!path) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

#ifdef UNICODE
	std::wstring wpath = path;
#else
	std::string ansiPath(path);
	std::wstring wpath(ansiPath.begin(), ansiPath.end());
#endif
	for (auto pattern : m_patternList) {
		if (wildcmp(pattern.c_str(), wpath.c_str())) {
			return true;
		}
	}

	return false;
}

bool ProcessList::update()
{
	std::vector<ProcessListItem> list;

	if (!getProcessList(list)) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Add missing processes to process map by ID
	for (auto p : list) {
		if (!m_processMap.count(p->id())) {
			m_processMap[p->id()] = p;

			m_dirty = true;
		}
	}

	// Remove terminated processes from map
	std::vector<DWORD> ids_to_remove;
	for (auto kv : m_processMap) {
		if (!kv.second->running()) {
			ids_to_remove.emplace_back(kv.first);
		}
	}

	for (auto pid : ids_to_remove) {
		m_processMap.erase(pid);

		m_dirty = true;
	}

	return true;
}

bool ProcessList::getProcessList(std::vector<ProcessListItem>& list)
{
	if (m_mode == RestartManager)
		return getProcessListFromRestartManager(list);
	else
		return getProcessListFromPsList(list);
}

bool ProcessList::getProcessListFromPsList(std::vector<ProcessListItem>& list)
{
	// Get process list
	return Process::queryAllProcesses(list, [this](Process& p) {
		return match(p.path());
	});
}

bool ProcessList::getProcessListFromRestartManager(std::vector<ProcessListItem>& list)
{
	std::vector<std::wstring> lockedFiles;

	std::vector<std::wstring> patternList;

	{
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		patternList = m_patternList;
	}

	for (auto& pattern : patternList) {
		auto full = std::filesystem::absolute(pattern);
		std::wstring filename = full.filename().wstring();
		std::wstring parentDir = full.parent_path().wstring();

		GetFilesByWildcard(parentDir, filename, lockedFiles);
	}

	return Process::queryAllProcesses(lockedFiles, list);
}

void ProcessList::thread(ProcessList* self)
{
	{
		// Warmup after initial update()

		std::unique_lock<std::mutex> lock(self->m_event_mutex);
		std::cv_status status = self->m_event.wait_for(lock, std::chrono::milliseconds(5000));
	}

	while (self->m_running) {
		auto start = std::chrono::system_clock::now();
		self->update();
		auto end = std::chrono::system_clock::now();

		std::chrono::milliseconds msec =
			std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		msec *= 5;

		if (msec.count() > 10000)
			msec = std::chrono::milliseconds(10000);
		else if (msec.count() < 1000)
			msec = std::chrono::milliseconds(1000);

		std::unique_lock<std::mutex> lock(self->m_event_mutex);
		std::cv_status status = self->m_event.wait_for(lock, msec);
	}

	std::unique_lock<std::mutex> lock(self->m_exit_event_mutex);
	self->m_exit_event.notify_one();
}

bool ProcessList::changed()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return m_dirty;
}

void ProcessList::fill(std::vector<ProcessListItem>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto kv : m_processMap) {
		output.emplace_back(kv.second);
	}

	m_dirty = false;
}
