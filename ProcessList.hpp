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
	ProcessList(ProcessListMode mode);
	~ProcessList();

	void addPattern(const TCHAR* pattern);

	bool changed();
	void fill(std::vector<ProcessListItem>& output);

private:
	bool match(const TCHAR* path);
	bool update();

	bool getProcessList(std::vector<ProcessListItem>& list);
	bool getProcessListFromPsList(std::vector<ProcessListItem>& list);
	bool getProcessListFromRestartManager(std::vector<ProcessListItem>& list);

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

	ProcessListMode m_mode;
};
