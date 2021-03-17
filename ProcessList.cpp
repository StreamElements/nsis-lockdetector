#include "stdafx.h"
#include "ProcessList.hpp"
#include <filesystem>
#include <windows.h>

ProcessList::ProcessList(std::shared_ptr<ProcessListProvider> provider) :
	m_dirty(false),
	m_running(true),
	m_provider(provider)
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

	if (m_thread.joinable()) {
		// Kill thread
		m_thread.std::thread::~thread();
	}

	/*
	{
		std::unique_lock<std::mutex> lock(m_exit_event_mutex);
		m_exit_event.wait(lock);
	}
	*/
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

bool ProcessList::update()
{
	std::vector<ProcessListItem> list;

	if (!m_provider->queryAllProcesses(m_patternList, m_currentContext)) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Add missing processes to process map by ID
	for (auto p : *m_currentContext->items()) {
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

void ProcessList::fill(std::shared_ptr<ProcessListContext>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	output = m_currentContext;

	m_dirty = false;
}
