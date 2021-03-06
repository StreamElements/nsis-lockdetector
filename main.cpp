#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>
#include <iostream>
#include <commctrl.h>
#include <strsafe.h>

#include "ProcessList.hpp"

#include "pluginapi.h"

// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
// and compile with -DPSAPI_VERSION=1

bool RunModalDialog(HWND hwndParent);

// NSIS vars
unsigned int g_stringsize;
stack_t **g_stacktop;
TCHAR *g_variables;

// Used in Dialog()
#ifdef UNICODE
std::vector<std::wstring> gPatterns;
#else
std::vector<std::string> gPatterns;
#endif

// Used in DlgProc and RunModalDialog
HINSTANCE gDllInstance;
ProcessListMode gMode = PsList;
ProcessList* gProcessList;
HIMAGELIST gImageList;
HICON gDefaultAppIcon;

// NSIS stack

const TCHAR* const NSISCALL popstring()
{
	stack_t *th;
	if (!g_stacktop || !*g_stacktop) return nullptr;
	th = (*g_stacktop);

	size_t strLen = _tcsclen(th->text);
	TCHAR* buf = new TCHAR[strLen + 1];
	_tcsncpy_s(buf, strLen + 1, th->text, strLen);
	buf[strLen] = 0;

	*g_stacktop = th->next;
	GlobalFree((HGLOBAL)th);

	return buf;
}

int NSISCALL popstring(TCHAR *str)
{
	stack_t *th;
	if (!g_stacktop || !*g_stacktop) return 1;
	th = (*g_stacktop);
	if (str) _tcsncpy_s(str, g_stringsize, th->text, g_stringsize);
	*g_stacktop = th->next;
	GlobalFree((HGLOBAL)th);
	return 0;
}

int NSISCALL popstringn(TCHAR *str, int maxlen)
{
	stack_t *th;
	if (!g_stacktop || !*g_stacktop) return 1;
	th = (*g_stacktop);
	if (str) _tcsncpy_s(str, maxlen ? maxlen : g_stringsize, th->text, g_stringsize);
	*g_stacktop = th->next;
	GlobalFree((HGLOBAL)th);
	return 0;
}

void NSISCALL pushstring(const TCHAR *str)
{
	stack_t *th;
	if (!g_stacktop) return;
	th = (stack_t*)GlobalAlloc(GPTR, (sizeof(stack_t) + (g_stringsize) * sizeof(TCHAR)));
	_tcsncpy_s(th->text, g_stringsize, str, g_stringsize);
	th->text[g_stringsize - 1] = 0;
	th->next = *g_stacktop;
	*g_stacktop = th;
}

TCHAR* NSISCALL getuservariable(const int varnum)
{
	if (varnum < 0 || varnum >= __INST_LAST) return NULL;
	return g_variables + varnum * g_stringsize;
}

void NSISCALL setuservariable(const int varnum, const TCHAR *var)
{
	if (var != NULL && varnum >= 0 && varnum < __INST_LAST)
		_tcsncpy_s(g_variables + varnum * g_stringsize, g_stringsize, var, g_stringsize);
}

///////////////////////////////////////////////////////////////////////
// API
///////////////////////////////////////////////////////////////////////

#define NSISFUNC(name) extern "C" void __declspec(dllexport) name(HWND hWndParent, int string_size, TCHAR* variables, stack_t** stacktop, extra_parameters* extra)

NSISFUNC(AddWildcardPattern)
{
	EXDLL_INIT();

	auto buf = popstring();

	if (buf) {
		gPatterns.emplace_back(buf);
		delete[] buf;
	}
}

NSISFUNC(SetMode)
{
	EXDLL_INIT();

	auto modeName = popstring();

	if (modeName) {
		if (_tcsicmp(modeName, TEXT("pslist")) == 0) {
			gMode = PsList;
		}
		else if (_tcsicmp(modeName, TEXT("restartmanager")) == 0) {
			gMode = RestartManager;
		}

		delete[] modeName;
	}
}

NSISFUNC(Dialog)
{
	EXDLL_INIT();

	bool dialogResult = false;

	gProcessList = new ProcessList(gMode);

	std::vector<std::wstring> patterns;
	for (auto pattern : gPatterns) {
#ifdef UNICODE
		patterns.emplace_back(std::wstring(pattern));
#else
		std::string s(pattern);
		patterns.emplace_back(std::wstring(s.begin(), s.end()));
#endif
	}
	gProcessList->addPatterns(patterns);

	// Get default app icon.
	if (0 == ExtractIconEx(TEXT("shell32.dll"), 2, NULL, &gDefaultAppIcon, 1)) {
		gDefaultAppIcon = nullptr;
	}

	gImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR32, 8, 16);

	dialogResult = RunModalDialog(hWndParent);

	if (gImageList) {
		ImageList_Destroy(gImageList);
	}

	if (gDefaultAppIcon) {
		DestroyIcon(gDefaultAppIcon);
	}

	delete gProcessList;

	if (!dialogResult) {
		pushstring(TEXT("error"));
	}
	else {
		pushstring(TEXT("OK"));
	}
}

int main(void)
{
	HWND hWndParent = NULL;

	gMode = RestartManager;
	gProcessList = new ProcessList(gMode);

	std::vector<std::wstring> patterns;
	//patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\*.exe");
	//patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\*.dll");
	patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\bin\\*.exe");
	patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\bin\\*.dll");
	patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\obs-plugins\\*.exe");
	patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\obs-plugins\\*.dll");
	patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\data\\*.exe");
	patterns.emplace_back(L"c:\\program files (x86)\\obs-studio\\data\\*.dll");
	gProcessList->addPatterns(patterns);

	// Get default app icon.
	ExtractIconEx(TEXT("shell32.dll"), 2, NULL, &gDefaultAppIcon, 1);
	gImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR32, 8, 16);

	RunModalDialog(hWndParent);

	ImageList_Destroy(gImageList);
	DestroyIcon(gDefaultAppIcon);

	delete gProcessList;

	return 0;
}

// InitListViewColumns: Adds columns to a list-view control.
// hWndListView:        Handle to the list-view control. 
// Returns TRUE if successful, and FALSE otherwise. 
BOOL InitListViewColumns(HWND hWndListView)
{
	const TCHAR* szText = TEXT("Program");
	LVCOLUMN lvc;
	int iCol;

	// Initialize the LVCOLUMN structure.
	// The mask specifies that the format, width, text,
	// and subitem members of the structure are valid.
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

	RECT rect;
	GetClientRect(hWndListView, &rect);

	// Add the columns.
	for (iCol = 0; iCol < 1; iCol++)
	{
		lvc.iSubItem = iCol;
		lvc.pszText = const_cast<TCHAR*>(szText);
		lvc.cx = rect.right - rect.left;               // Width of column in pixels.

		if (iCol < 2)
			lvc.fmt = LVCFMT_LEFT;  // Left-aligned column.
		else
			lvc.fmt = LVCFMT_RIGHT; // Right-aligned column.

		// Insert the columns into the list view.
		if (ListView_InsertColumn(hWndListView, iCol, &lvc) == -1)
			return FALSE;
	}

	return TRUE;
}

void EnsureListViewItemCount(HWND hwndList, size_t itemCount)
{
	LVITEM lvi;

	// Initialize LVITEM members that are common to all items.
	lvi.pszText = LPSTR_TEXTCALLBACK; // Sends an LVN_GETDISPINFO message.
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
	lvi.stateMask = 0;
	lvi.iSubItem = 0;
	lvi.state = 0;

	// Initialize LVITEM members that are different for each item.
	for (size_t index = ListView_GetItemCount(hwndList); index < itemCount; ++index)
	{
		lvi.iItem = (int)index;
		lvi.iImage = (int)index;

		// Insert items into the list.
		ListView_InsertItem(hwndList, &lvi);
	}

	size_t currentCount = ListView_GetItemCount(hwndList);
	while (currentCount > itemCount) {
		ListView_DeleteItem(hwndList, currentCount - 1);
		
		--currentCount;
	}
}

void SetImageListIcons(std::vector<ProcessListItem> process_list)
{
	ImageList_SetImageCount(gImageList, 0);
	for (size_t index = 0; index < process_list.size(); ++index)
	{
		if (process_list[index]->icon()) {
			ImageList_AddIcon(gImageList, process_list[index]->icon());
		}
		else {
			ImageList_AddIcon(gImageList, gDefaultAppIcon);
		}
	}
}

LRESULT CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const UINT TIMER_ID = 1;

	static std::vector<ProcessListItem> process_list;

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		SetTimer(hDlg, TIMER_ID, 1000, NULL);

		gProcessList->fill(process_list);

		HWND hwndList = GetDlgItem(hDlg, IDC_LIST);
		
		ListView_SetImageList(hwndList, gImageList, LVSIL_SMALL);
		InitListViewColumns(hwndList);
		EnsureListViewItemCount(hwndList, process_list.size());
		SetImageListIcons(process_list);

		// Set input focus to the list box.
		SetFocus(hwndList);

		// Set icons
		const HANDLE hbicon = ::LoadImage(
			::GetModuleHandle(0),
			MAKEINTRESOURCE(IDI_ICON),
			IMAGE_ICON,
			::GetSystemMetrics(SM_CXICON),
			::GetSystemMetrics(SM_CYICON),
			0);
		if (hbicon) {
			::SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hbicon);
		}

		const HANDLE hsicon = ::LoadImage(
			::GetModuleHandle(0),
			MAKEINTRESOURCE(IDI_ICON),
			IMAGE_ICON,
			::GetSystemMetrics(SM_CXSMICON),
			::GetSystemMetrics(SM_CYSMICON),
			0);
		if (hsicon) {
			::SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hsicon);
		}

		return TRUE;
	}

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case LVN_GETDISPINFO:
			NMLVDISPINFO * plvdi = (NMLVDISPINFO*)lParam;

			switch (plvdi->item.iSubItem)
			{
			case 0:
				if ((size_t)plvdi->item.iItem < process_list.size()) {
					auto process = process_list[plvdi->item.iItem];

					if (process->mainWindowTitle()) {
						StringCchCopy(plvdi->item.pszText, plvdi->item.cchTextMax, process->mainWindowTitle());
					}
					else {
						StringCchCopy(plvdi->item.pszText, plvdi->item.cchTextMax, process->path());
						//plvdi->item.pszText =
						//	const_cast<TCHAR*>(process_list[plvdi->item.iItem]->path());
					}
				}
				break;
			}
			break;
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			int mbResult =
				::MessageBox(
					hDlg,
					TEXT("Closing the running programs may cause you to lose data. Are you sure?"),
					TEXT("Close running programs?"),
					MB_YESNO | MB_ICONWARNING);

			if (mbResult == IDYES) {
				EndDialog(hDlg, LOWORD(wParam));
			}
			return TRUE;
		}
		case IDCANCEL:
		{
			int mbResult =
				::MessageBox(
					hDlg,
					TEXT("Are you sure you wish to cancel setup?"),
					TEXT("Cancel setup?"),
					MB_YESNO | MB_ICONQUESTION);

			if (mbResult == IDYES) {
				EndDialog(hDlg, LOWORD(wParam));
			}
			return TRUE;
		}
		}
		break;

	case WM_DESTROY:
		// Perform cleanup tasks. 
		KillTimer(hDlg, TIMER_ID);

		//PostQuitMessage(0);
		break;

	case WM_CLOSE:
		break;

	case WM_TIMER:
		if (gProcessList->changed()) {
			process_list.clear();
			gProcessList->fill(process_list);

			HWND hwndList = GetDlgItem(hDlg, IDC_LIST);
			EnsureListViewItemCount(hwndList, process_list.size());
			SetImageListIcons(process_list);
			InvalidateRect(hwndList, NULL, TRUE);

			if (!process_list.size()) {
				// No locking processes left, end dialog as if the user asked to kill all processes
				EndDialog(hDlg, IDOK);
			}
		}
		break;
	}

	return FALSE;
}

bool RunModalDialog(HWND hwndParent)
{
	std::vector<ProcessListItem> process_list;

	process_list.clear();
	gProcessList->fill(process_list);

	/*
	TCHAR buf[1024];
	_stprintf_s(buf, 1024, TEXT("process_list.size(): %d"), process_list.size());
	::MessageBox(hwndParent, buf, TEXT("Dialog"), 0);
	*/

	if (!process_list.size()) {
		// No locking processes
		return true;
	}

	UINT nResult =
		(UINT)DialogBox(gDllInstance, MAKEINTRESOURCE(IDD_ListDialog), hwndParent, (DLGPROC)DlgProc);

	if (nResult == IDOK) {
		process_list.clear();
		gProcessList->fill(process_list);

		for (auto process : process_list) {
			bool result = process->terminate(255);
		}

		return true;
	}
	else {
		return false;
	}
}

BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	if (hinstDLL) {
		gDllInstance = hinstDLL;
	}

	return TRUE;
}