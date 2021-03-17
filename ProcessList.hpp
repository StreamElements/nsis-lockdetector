#pragma once

#include "Process.hpp"

#include <vector>
#include <map>
#include <mutex>
#include <thread>

typedef std::shared_ptr<Process> ProcessListItem;

enum ProcessListMode
{
	PsList,
	RestartManager
};

class ProcessList
{
public:
	ProcessList(std::shared_ptr<ProcessListProvider> provider);
	~ProcessList();

	void addPatterns(const std::vector<std::wstring>& patterns);
	void addPattern(const TCHAR* pattern);

	bool changed();
	void fill(std::shared_ptr<ProcessListContext>& output);

private:
	bool update();

	static void thread(ProcessList* self);

private:
	bool m_dirty;
	std::map<DWORD, ProcessListItem> m_processMap;
	std::vector<std::wstring> m_patternList;
	std::recursive_mutex m_mutex;
	std::thread m_thread;
	
	std::condition_variable m_event;
	std::mutex m_event_mutex;

	std::condition_variable m_exit_event;
	std::mutex m_exit_event_mutex;

	bool m_running;

	std::shared_ptr<ProcessListProvider> m_provider = nullptr;
	std::shared_ptr<ProcessListContext> m_currentContext = nullptr;
};
