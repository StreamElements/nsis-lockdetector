#include "stdafx.h"
#include "ProcessList.hpp"
#include <iostream>

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


ProcessList::ProcessList() :
	m_dirty(false),
	m_running(true)
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

void ProcessList::addPattern(const TCHAR const * pattern)
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

bool ProcessList::match(const TCHAR const* path)
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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	std::vector<std::shared_ptr<Process>> list;

	// Get process list
	if (!Process::queryAllProcesses(list, [this](Process& p) {
		return match(p.path());
	})) {
		return false;
	}

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

void ProcessList::thread(ProcessList* self)
{
	while (self->m_running) {
		self->update();

		std::unique_lock<std::mutex> lock(self->m_event_mutex);
		std::cv_status status = self->m_event.wait_for(lock, std::chrono::milliseconds(1000));
	}

	std::unique_lock<std::mutex> lock(self->m_exit_event_mutex);
	self->m_exit_event.notify_one();
}

bool ProcessList::changed()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return m_dirty;
}

void ProcessList::fill(std::vector<std::shared_ptr<Process>>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto kv : m_processMap) {
		output.emplace_back(kv.second);
	}

	m_dirty = false;
}
