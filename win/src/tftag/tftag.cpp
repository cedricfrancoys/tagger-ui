/* tftag.cpp - win32 application for tagging files.

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tftag.h" 
#include "../commons/eventlistener.h" 
#include "../commons/dosexec.h" 

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")
#pragma comment(lib, "uxtheme.lib")

#define FILE_NAME_MAX 1024


// Global variables
HINSTANCE hInst;
HWND hPaneTags, hPaneFiles, hPaneLogs;
WCHAR* taggerCommandLinePath = NULL;

HANDLE hSharedMemory;
LPVOID lpMapAddress = NULL;


/* Read registry to fetch path of installation directory.
Set taggerCommandLinePath according to HKLM/SOFTWARE/TaggerUI/Tagger_Dir.
*/
int getEnv() {
	HKEY  hKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\TaggerUI", 0, KEY_READ, &hKey);

	DWORD type, size = FILE_NAME_MAX;
	LPWSTR data = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*size);
	if( RegQueryValueEx(hKey, L"Tagger_Dir", 0, &type, (LPBYTE) data, &size) != ERROR_SUCCESS ) return 0;

//	taggerCommandLinePath = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data)+wcslen(L"\\tagger.exe --quiet")+1) );
//	wsprintf(taggerCommandLinePath, L"%s\\tagger.exe --quiet", data);
	taggerCommandLinePath = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(data)+wcslen(L"\\tagger.exe")+1) );
	wsprintf(taggerCommandLinePath, L"%s\\tagger.exe", data);

	LocalFree(data);
	RegCloseKey(hKey);
	return 1;
}



// Forward declarations of functions included in this module

// functions that will be binded to the event listener
void initDialog(HWND, WPARAM, LPARAM);
void closeDialog(HWND, WPARAM, LPARAM);
void changeTab(HWND, WPARAM, LPARAM);

void removeTags(HWND, WPARAM, LPARAM);
void addTags(HWND, WPARAM, LPARAM);
void updateTagName(HWND, WPARAM, LPARAM);
void updateFilesList(HWND, WPARAM, LPARAM);
void notifyIcon(HWND, WPARAM, LPARAM);

void addLog(LPWSTR str, BOOL isCommand=false);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
	// copy hInstance to the global scope
	hInst = hInstance;
	// register a custom message for files list update
	DWORD WM_FLUPDATE = RegisterWindowMessage(L"TaggerFilesListUpdate");

	// try to open named shared memory
	if ( (hSharedMemory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, FILE_NAME_MAX, L"TaggerUIMappedMem")) == NULL) { 
		MessageBox(NULL, L"Could not create file-mapping object.", L"error", MB_OK | MB_ICONERROR); 
	}
	else {
		lpMapAddress = MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0); 
		if(GetLastError() == ERROR_ALREADY_EXISTS) {
			// main window already exists : send it current filename, if any
			// retrieve command line arguments
			int argc;
			LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);
			if (argc >= 2) { 
				// copy current argv value to shared memory
				wsprintf((LPWSTR) lpMapAddress, argv[1]);
				// send broadcast message to notify change
				SendMessage(HWND_BROADCAST, WM_FLUPDATE, 0, 0);
			}
			UnmapViewOfFile(lpMapAddress);			
			CloseHandle(hSharedMemory);		
			return 0;
		}
	}


	INITCOMMONCONTROLSEX InitCtrlEx;
	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_BAR_CLASSES | ICC_TAB_CLASSES;
	InitCommonControlsEx(&InitCtrlEx);

	WNDCLASSEX wcex;
	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.style			= CS_CLASSDC;
	wcex.lpfnWndProc	= EventListener::wndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDD_ICON));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;
	wcex.lpszClassName	= L"myClass";
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDD_ICON));
	RegisterClassEx(&wcex);

	HWND phWnd = CreateWindowEx(WS_EX_TOOLWINDOW, L"myClass", L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);




	HWND hSplash = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_SPLASH), phWnd, (DLGPROC) NULL, 0);
    ShowWindow(hSplash, SW_SHOW);
    UpdateWindow(hSplash);

	HWND hWnd;
    if (!(hWnd = CreateDialogParamW(hInstance, MAKEINTRESOURCE(IDD_DIALOG), phWnd, (DLGPROC) EventListener::dlgProc, 0))) {
        MessageBox(NULL, L"App creation failed!", L"Tagger", MB_OK | MB_ICONERROR);
        return 1;
    }

	// create status bar notify icon 
	DWORD WM_NOTIFYICON = RegisterWindowMessage(L"TaggerNotifyIcon");
	HICON hIcon = (HICON) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ICON));
    NOTIFYICONDATA tnid;  
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = hWnd; 
    tnid.uID = 0; 
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
    tnid.uCallbackMessage = WM_NOTIFYICON; 
    tnid.hIcon = hIcon; 
    wcscpy(tnid.szTip, L"TaggerUI monitor");
    Shell_NotifyIcon(NIM_ADD, &tnid); 

	// create tabControl panes 
	HWND hTabControl = GetDlgItem(hWnd, IDC_TAB);
	hPaneTags = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_PANE_ONE), hTabControl, (DLGPROC) EventListener::dlgProc, 0);
	hPaneFiles = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_PANE_TWO), hTabControl, (DLGPROC) EventListener::dlgProc, 0);
	hPaneLogs = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_PANE_THREE), hTabControl, (DLGPROC) EventListener::dlgProc, 0);


	// since WM_INITDIALOG has been sent before binding, we need to initialize dialog manually
	initDialog(hWnd, (WPARAM) 0, (LPARAM) 0);

	// bind events we're interested in with appropriate handling functions
	EventListener* eventListener = EventListener::getInstance(HWND_DIALOG);
	// global events
	eventListener->bind(hWnd, 0, WM_CLOSE, closeDialog);
	eventListener->bind(hWnd, 0, WM_FLUPDATE, updateFilesList);
	eventListener->bind(hWnd, 0, WM_NOTIFYICON, notifyIcon);
	
	// handle dialog default IDOK action to terminate app
	eventListener->bind(hWnd, IDOK, BN_CLICKED, closeDialog);	

	// controls events
	eventListener->bind(hWnd, IDC_TAB, TCN_SELCHANGE, changeTab);	
	eventListener->bind(hPaneTags, ID_REMOVE, BN_CLICKED, removeTags);
	eventListener->bind(hPaneTags, ID_ADD, BN_CLICKED, addTags);
	eventListener->bind(hPaneTags, ID_TAGNAME, EN_CHANGE, updateTagName);

	ShowWindow(phWnd, SW_HIDE);
	ShowWindow(hSplash, SW_HIDE);
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // Main message loop:
	MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if(!IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }

    return (int) msg.wParam;
}


void initDialog(HWND hWnd, WPARAM, LPARAM) {
	// populate tabControl with custom tabs
	TCITEM TabCtrlItem;
	WCHAR szText[100];
	TabCtrlItem.mask = TCIF_TEXT;
    TabCtrlItem.pszText = szText;
	LoadString(hInst, IDD_PANE_ONE, szText, sizeof(szText));
	SendMessage(GetDlgItem( hWnd, IDC_TAB ), TCM_INSERTITEM, 0, (LPARAM) &TabCtrlItem);
    LoadString(hInst, IDD_PANE_TWO, szText, sizeof(szText));
	SendMessage(GetDlgItem( hWnd, IDC_TAB ), TCM_INSERTITEM, 1, (LPARAM) &TabCtrlItem);
    LoadString(hInst, IDD_PANE_THREE, szText, sizeof(szText));
	SendMessage(GetDlgItem( hWnd, IDC_TAB ), TCM_INSERTITEM, 2, (LPARAM) &TabCtrlItem);

	EnableThemeDialogTexture(hPaneTags, ETDT_ENABLETAB);	
	EnableThemeDialogTexture(hPaneFiles, ETDT_ENABLETAB);	
	EnableThemeDialogTexture(hPaneLogs, ETDT_ENABLETAB);	

	// set log editControl font
	LOGFONT logfont;
	memset(&logfont, 0, sizeof(logfont));
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfWeight = FW_NORMAL;
	logfont.lfHeight = -MulDiv(9.5, GetDeviceCaps(GetDC(hWnd), LOGPIXELSY), 72);
	logfont.lfOutPrecision = OUT_RASTER_PRECIS;
	//lstrcpy(logfont.lfFaceName, L"Lucida Console");
	lstrcpy(logfont.lfFaceName, L"Consolas");
	HFONT hFont = CreateFontIndirect(&logfont); 
	SendMessage(GetDlgItem( hPaneLogs, ID_LOG ), WM_SETFONT, (WPARAM)hFont, TRUE);

	// retrieve command line arguments
    int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);

	if (argv == NULL || argc < 2) { 
		MessageBox(NULL, L"A filename with full path is expected as first argument of this app.", L"Notice", MB_OK | MB_ICONERROR);
		// closeDialog(hWnd);
	}
	else {
		// add filename to selectedfiles array
		SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_ADDSTRING, 0, (LPARAM) (LPWSTR) argv[1]);

		// set filename, according to received argument
		SetDlgItemText(hPaneTags, ID_FILENAME, argv[1]);

		// enable '+' and '-' buttons
		EnableWindow( GetDlgItem( hPaneTags, ID_REMOVE ), TRUE);
		EnableWindow( GetDlgItem( hPaneTags, ID_ADD ), TRUE);

		// try to retrieve list of available tags
		if(!getEnv()) {
			MessageBox(NULL, L"Unable to locate installation directory.\nTo solve this, try re-installing the application.", L"Error", MB_OK|MB_ICONERROR);
		}
		else {
			// populate tags lists
			LPWSTR command, output;
			// 1) retrieve all existing tags 
			command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tags")+1) );
			wsprintf(command, L"%s tags", taggerCommandLinePath);
			output = DosExec(command);
			addLog(command, true);
			addLog(output);

			for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
				line[wcslen(line)-1] = '\0';
				SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_MATCH, LB_ADDSTRING, 0, (LPARAM) line);
				SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) line);
			}
			LocalFree(command);
			LocalFree(output);
			// 2) retrieve tags already applied on the given file
			command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" query ")+wcslen(L"\"\"")+wcslen(argv[1])+1) );
			wsprintf(command, L"%s query \"%s\"", taggerCommandLinePath, argv[1]);		
			output = DosExec(command);

			addLog(command, true);
			addLog(output);

			if(wcscmp(output, L"No tag currently applied on given file(s).\r\n") != 0) {
				for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
					line[wcslen(line)-1] = '\0';
					SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_SET, LB_ADDSTRING, 0, (LPARAM) line);
					// remove tag from ID_LIST_TAGS_MATCH and ID_LIST_TAGS_ALL
					int index = SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_ALL, LB_FINDSTRING, (WPARAM) 0, (LPARAM) line);
					SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_MATCH, LB_DELETESTRING, (WPARAM) index, 0);
					SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_ALL, LB_DELETESTRING, (WPARAM) index, 0);
				}
			}
			LocalFree(command);
			LocalFree(output);
		}
	}
	LocalFree(argv);
}

void removeTags(HWND hWnd, WPARAM, LPARAM){
	int sel_count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETSELCOUNT, 0, 0);
	INT* indexes = (INT*) LocalAlloc(LPTR, sizeof(INT) * sel_count);
	SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETSELITEMS, (WPARAM) sel_count, (LPARAM) indexes);

	// first pass : update database
	for(int i = 0; i < sel_count; ++i) {
		int tagname_len = SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETTEXTLEN, (WPARAM) indexes[i], 0);
		LPWSTR tagname = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(tagname_len+1));
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETTEXT, (WPARAM) indexes[i], (LPARAM) tagname);
// todo : we should check the current pattern in ID_TAG_NAME
		// add selected tags to ID_LIST_TAGS_MATCH and ID_LIST_TAGS_ALL
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_ADDSTRING, 0, (LPARAM) tagname);
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) tagname);
		// remove tag from files
		int files_count = SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETCOUNT, 0, 0);
		for(int j = 0; j < files_count; ++j) {
			// retrieve filename
			int filename_len = SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETTEXTLEN, (WPARAM) j, 0);
			LPWSTR filename = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(filename_len+1));
			SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETTEXT, (WPARAM) j, (LPARAM) filename);
			// call DOS command (tagger tag -{tagname} file}
			LPWSTR output, command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tag -")+tagname_len+wcslen(L" \"\"\"\"")+filename_len+1) );
			wsprintf(command, L"%s tag -\"%s\" \"%s\"", taggerCommandLinePath, tagname, filename);
			output = DosExec(command);
			addLog(command, true);
			addLog(output);

			LocalFree(output);
			LocalFree(command);
			LocalFree(filename);
		}
		free(tagname);
	}
	// second pass : remove selected items from ID_LIST_TAGS_SET
	int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETCOUNT, 0, 0);
	for(int i = count-1; i >= 0; --i) {
		if(SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_GETSEL, (WPARAM) i, 0)) {
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_DELETESTRING, (WPARAM) i, 0);
		}
	}
	LocalFree(indexes);
}

/* Add selected tag to given file.
 Update available tags and set tags lists.
*/
void addTags(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// retrieve selected tags
	int sel_count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETSELCOUNT, 0, 0);
	if(sel_count == 0){
		// no tag is selected
		int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
		if(len == 0) {
			// no input pattern is given : do nothing
			return;
		}
		else {
			// count number of partial match tags
			int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETCOUNT, 0, 0);
			if(count == 0) {
				int tagname_len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
				LPWSTR tagname = (LPWSTR) malloc(sizeof(WCHAR)*(tagname_len+1));
				GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) tagname, tagname_len+1);
				// check if tag is already in file's tag list								
				if(SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_FINDSTRING, 0, (LPARAM) tagname) == LB_ERR) {
					// create a new tag
					LPWSTR command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" create +\"\"")+tagname_len+1) );
					wsprintf(command, L"%s create \"%s\"", taggerCommandLinePath, tagname);
					DosExec(command);
					LocalFree(command);
					// add item to global and available tags lists
					SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) tagname);
					int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_ADDSTRING, 0, (LPARAM) tagname);
					SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_SELITEMRANGE, TRUE, (LPARAM) MAKELPARAM(index, index));
					addTags(hWnd, wParam, lParam);
				}
			}
			else {
				// input pattern is not among remaining items
				// this should never occur since exact match item is automaticaly selected at input update
			}
		}
	}
	else {
		// at least one tag is selected		
		int* indexes = (int*) LocalAlloc(LPTR, sizeof(int)*sel_count);
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETSELITEMS, (WPARAM) sel_count, (LPARAM) indexes);
		// first pass : update database
		for(int i = 0; i < sel_count; ++i) {
			// get string for current index
			int tagname_len = SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETTEXTLEN, (WPARAM) indexes[i], 0);
			LPWSTR tagname = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(tagname_len+1));
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETTEXT, (WPARAM) indexes[i], (LPARAM) tagname);
			// add selected tags to ID_LIST_TAGS_SET
			SendDlgItemMessage(hWnd, ID_LIST_TAGS_SET, LB_ADDSTRING, 0, (LPARAM) tagname);

			// call DOS command (tagger tag +{new_tag} file}
			int files_count = SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETCOUNT, 0, 0);
			for(int j = 0; j < files_count; ++j) {
				// retrieve filename
				int filename_len = SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETTEXTLEN, (WPARAM) j, 0);
				LPWSTR filename = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*(filename_len+1));
				SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETTEXT, (WPARAM) j, (LPARAM) filename);
				// call DOS command (tagger tag -{tagname} file}
				LPWSTR output, command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" tag +")+tagname_len+wcslen(L" \"\"\"\"")+filename_len+1) );
				wsprintf(command, L"%s tag +\"%s\" \"%s\"", taggerCommandLinePath, tagname, filename);
				output = DosExec(command);
				addLog(command, true);
				addLog(output);

				LocalFree(output);
				LocalFree(command);
				LocalFree(filename);
			}
			LocalFree(tagname);
		}
		// second pass : remove selected items from ID_LIST_TAGS_MATCH and ID_LIST_TAGS_ALL
		int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETCOUNT, 0, 0);
		for(int i = count-1; i >= 0; --i) {
			if(SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_GETSEL, (WPARAM) i, 0)) {
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_DELETESTRING, (WPARAM) i, 0);
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_DELETESTRING, (WPARAM) i, 0);
			}
		}
		LocalFree(indexes);
	}
}

/* Limit the list of available tags to those which name match the current pattern
*/
void updateTagName(HWND hWnd, WPARAM, LPARAM) {
	// reset available list
	SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_RESETCONTENT, 0, 0);

	// retrieve input pattern
	int len = SendDlgItemMessage(hWnd, ID_TAGNAME, WM_GETTEXTLENGTH, 0, 0);
	LPWSTR pattern = (LPWSTR) malloc(sizeof(WCHAR)*(len+1));
	GetDlgItemText(hWnd, ID_TAGNAME, (LPWSTR) pattern, len+1);

	// retrieve global list count
	LPWSTR str = (LPWSTR) malloc(sizeof(WCHAR)*1024);
	int count = SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_GETCOUNT, 0, 0);

	// add only tags matching input pattern
	for(int i = 0; i < count; ++i) {
		SendDlgItemMessage(hWnd, ID_LIST_TAGS_ALL, LB_GETTEXT, (WPARAM) i, (LPARAM) str);
		if(wcsstr(str, pattern) == str || wcslen(pattern) == 0) {
			int index = SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_ADDSTRING, 0, (LPARAM) str);
			if(wcscmp(str, pattern) == 0) {
				SendDlgItemMessage(hWnd, ID_LIST_TAGS_MATCH, LB_SELITEMRANGE, TRUE, (LPARAM) MAKELPARAM(index, index));
			}
		}
	}
}

/* Update filesList and counter.
 Set tags list will keep only tags applied on all selected files.
*/
void updateFilesList(HWND hWnd, WPARAM, LPARAM) {
	// MessageBox(NULL, (LPWSTR) lpMapAddress, L"info", MB_OK);

	// ensure filename is not already in the list
	if(SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_FINDSTRING, (WPARAM) 0, (LPARAM) (LPWSTR) lpMapAddress) != LB_ERR) {
		return;
	}

 	// add filename to selectedFiles array
	SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_ADDSTRING, 0, (LPARAM) (LPWSTR) lpMapAddress);

	// update filename text
	int filesCount = SendDlgItemMessage(hPaneFiles, ID_LIST_FILES, LB_GETCOUNT, 0, 0);
	LPWSTR str = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR)*FILE_NAME_MAX);
	wsprintf(str, L"%d selected files", filesCount);
	SetDlgItemText(hPaneTags, ID_FILENAME, str);


	// reset ID_LIST_TAGS_TMP
   	SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_TMP, LB_RESETCONTENT, 0, 0);
	
	// run command to get all tags for new file
	LPWSTR command = (LPWSTR) LocalAlloc(LPTR, sizeof(WCHAR) * (wcslen(taggerCommandLinePath)+wcslen(L" query ")+wcslen(L"\"\"")+wcslen((LPWSTR)lpMapAddress)+1) );
	wsprintf(command, L"%s query \"%s\"", taggerCommandLinePath, (LPWSTR) lpMapAddress);
	LPWSTR output = DosExec(command);
	addLog(command, true);
	addLog(output);

	if(wcscmp(output, L"No tag currently applied on given file(s).\r\n") != 0) {
		for(LPWSTR line = wcstok(output, L"\n"); line; line = wcstok(NULL, L"\n")) {
			line[wcslen(line)-1] = '\0';
			SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_TMP, LB_ADDSTRING, 0, (LPARAM) line);
		}
	}
	LocalFree(command);
	LocalFree(output);
   	
	// add kept tags	
	int count = SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_SET, LB_GETCOUNT, 0, 0);
	for(int i = count-1; i >= 0; --i) {		
		SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_SET, LB_GETTEXT, (WPARAM) i, (LPARAM) str);
		// check if tag is also present in ID_LIST_TAG_TMP
		int index = SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_TMP, LB_FINDSTRING, 0, (LPARAM) str);
		if(index == LB_ERR) {
			// tag is not common to previously selected and newly added file
			SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_SET, LB_DELETESTRING, (WPARAM) i, 0);
			SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_MATCH, LB_ADDSTRING, 0, (LPARAM) str);
			SendDlgItemMessage(hPaneTags, ID_LIST_TAGS_ALL, LB_ADDSTRING, 0, (LPARAM) str);
		}				
	}
	LocalFree(str);
}


void notifyIcon(HWND hWnd, WPARAM wParam, LPARAM lParam) {	
	if ((UINT) lParam == WM_RBUTTONDOWN) {
		HMENU hMenu = GetSubMenu( LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_POPUP_MENU)), 0);
		POINT mPos;
		GetCursorPos(&mPos);
		TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN, mPos.x, mPos.y, 0, hWnd, NULL); 
	}
}
void closeDialog(HWND hWnd, WPARAM, LPARAM) {
	if(taggerCommandLinePath != NULL) LocalFree(taggerCommandLinePath);
	if(lpMapAddress != NULL) UnmapViewOfFile(lpMapAddress);
	if(hSharedMemory != NULL) CloseHandle(hSharedMemory);

    NOTIFYICONDATA tnid;  
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = hWnd; 
    tnid.uID = 0;         
    Shell_NotifyIcon(NIM_DELETE, &tnid); 

	DestroyWindow(hWnd);
	PostQuitMessage(0);
}

void changeTab(HWND hWnd, WPARAM, LPARAM) {
	HWND hTabControl = GetDlgItem(hWnd, IDC_TAB);
	HWND hNewPane, hCurrentPane = GetTopWindow(hTabControl);
	int selectedTab = TabCtrl_GetCurSel(hTabControl);
	switch(selectedTab) {
	case 0:
		hNewPane = hPaneTags;
		break;
	case 1:
		hNewPane = hPaneFiles;
		break;
	case 2:
		hNewPane = hPaneLogs;
		break;

	}
	if(!IsWindowVisible(hNewPane)) {
		ShowWindow(hNewPane, SW_SHOW);
	}
	BringWindowToTop(hNewPane);
	UpdateWindow(hNewPane);
}

void addLog(LPWSTR str, BOOL isCommand) {
	if(isCommand) SendDlgItemMessage(hPaneLogs, ID_LOG, EM_REPLACESEL, 0, (LPARAM) (LPWSTR) L"$>");
	SendDlgItemMessage(hPaneLogs, ID_LOG, EM_REPLACESEL, 0, (LPARAM) (LPWSTR) str);
	SendDlgItemMessage(hPaneLogs, ID_LOG, EM_REPLACESEL, 0, (LPARAM) (LPWSTR) L"\r\n");
}