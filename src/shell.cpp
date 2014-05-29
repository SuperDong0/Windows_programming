#define _CRT_SECURE_NO_WARNINGS

#include "resource.h"
#include "my_dll.h"
#include "helper.h"
#include "getopt.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <clocale>
#include <windows.h>
#include <Shlobj.h>
#include <process.h>
#include <Wininet.h>
#include <Shlwapi.h>
#include <Olectl.h>
#include <Imagehlp.h>
#include <initguid.h>
#include "my_dll_iid.h"

using std::vector;
using std::string;

void RestartShell();

//-------------------------------------------------
//                      CH 3
//-------------------------------------------------

// Link with shell32.lib, Dbghelp.lib
void TestSHFileOperation()
{
        SHCreateDirectoryEx(NULL, _T("test\\kerker\\"), NULL);
        
        SHFILEOPSTRUCT file_op = {};

        file_op.wFunc  = FO_DELETE;
        file_op.fFlags = FOF_ALLOWUNDO | FOF_FILESONLY;
        file_op.pFrom  = _T("C:\\Documents and Settings\\lausai\\My Documents\\Dropbox\\my_code\\windows_shell\\test\\*.*\0");

        int result = SHFileOperation(&file_op);
        
        if (0 != result && !file_op.fAnyOperationsAborted)
                PrintError();
}


//-------------------------------------------------
//                      CH 4
//-------------------------------------------------
// Link with shell32.lib
// Example:
//      TestSHGetFileInfo(_T("shell.exe"), SHGFI_EXETYPE)
void TestSHGetFileInfo(const TCHAR* file_path, UINT flags)
{
        SHFILEINFO file_info = {};

        DWORD_PTR result = SHGetFileInfo(file_path, 0, &file_info, sizeof(SHFILEINFO), SHGFI_TYPENAME | flags);
        
        const WORD PE = 0x4550;
        const WORD MZ = 0x5A4D;

        if (SHGFI_EXETYPE | flags) {
                if (0 == result)
                        puts("not executable");
                else if (PE == LOWORD(result) && 0 == HIWORD(result))
                        puts("console application or .bat file");
                else if (MZ == LOWORD(result) && 0 == HIWORD(result))
                        puts("MS-DOS exe or .com file");
                else
                        puts("windows application");
        }

        _tprintf(_T("file type=%ls\n"), file_info.szTypeName);
}


//-------------------------------------------------
//                      CH 5
//-------------------------------------------------

// Link with user32.lib
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM lpData)
{
        switch (uMsg) {
        case BFFM_INITIALIZED:
                printf("initial selected folder=%s\n", lpData);
                SendMessage(hwnd, BFFM_SETSELECTION, true, lpData);
                break;
        default:
                ;
        }

        return 0;
}

HRESULT pathToPidl(const TCHAR* path, PIDLIST_ABSOLUTE& pidl)
{
        IShellFolder* pIshell_folder = NULL;

        HRESULT ret = SHGetDesktopFolder(&pIshell_folder);

        if (FAILED(ret)) return ret;
        
        WCHAR w_path[MAX_PATH] = {};
        ToWideChar(path, w_path, MAX_PATH);
        
        ULONG chars_parsed = 0;
        ret = pIshell_folder->ParseDisplayName(NULL, NULL, w_path, &chars_parsed, &pidl, NULL);
        pIshell_folder->Release();

        return ret;
}

// Link with shell32.lib
void TestSHBrowseForFolder()
{
        BROWSEINFO browse_info        = {};
        TCHAR file_selected[MAX_PATH] = {};

        browse_info.pszDisplayName = file_selected;
        browse_info.lpszTitle      = _T("shell function test");
        browse_info.ulFlags        = BIF_BROWSEINCLUDEFILES | BIF_EDITBOX;
        browse_info.lParam         = reinterpret_cast<LPARAM>(_T("C:\\Documents and Settings\\lausai\\My Documents\\Dropbox\\"));
        browse_info.lpfn           = BrowseCallbackProc;

        PIDLIST_ABSOLUTE pidl = NULL;
        
        puts("if set root node to my computer, enter 0, else set root node to C:\\");

        int root_node = 0;
        scanf("%d", &root_node);

        if (root_node == 0)
                SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);
        else
                pathToPidl(_T("C:\\"), pidl);

        browse_info.pidlRoot = pidl;

        pidl = SHBrowseForFolder(&browse_info);
        
        puts("");

        if (pidl) {
                TCHAR file_path[128] = {};
                SHGetPathFromIDList(pidl, file_path);

                _tprintf(_T("full path=%s\n"), file_path);
                _tprintf(_T("file name=%s\n"), browse_info.pszDisplayName);
                _tprintf(_T("image index=%d\n"), browse_info.iImage);

                CoTaskMemFree(pidl);
        } else {
                _tprintf(_T("pidl is NULL\n"));
        }
}


//-------------------------------------------------
//                      CH 6
//-------------------------------------------------

// Link with old32.lib
void PrintShortcutPath(const TCHAR* shortcut_path)
{
        CoInitialize(NULL);

        IShellLink* shell_link = NULL;

        HRESULT result = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IShellLink, reinterpret_cast<LPVOID*>(&shell_link));

        if (FAILED(result)) {
                puts("create IShellLink fail!");
                return;
        }

        IPersistFile* pf = NULL;
        // Use for loop instead of while (true) to avoid warning
        for (;;) {
                result = shell_link->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID*>(&pf));
                if (FAILED(result)) {
                        puts("QueryInterface fail!");
                        break;
                }
                
                WCHAR wide_file_path[MAX_PATH] = {};

                ToWideChar(shortcut_path, wide_file_path, MAX_PATH);
                result = pf->Load(wide_file_path, STGM_READ);

                if (FAILED(result)) {
                        puts("Load shortcut fail!");
                        break;
                }
        
                result = shell_link->Resolve(NULL, SLR_ANY_MATCH);
                if (FAILED(result)) {
                        puts("resolve fail!");
                        break;
                }

                TCHAR file_path[MAX_PATH] = {};
        
                shell_link->GetPath(file_path, MAX_PATH, NULL, SLGP_SHORTPATH);
                _tprintf(_T("path=%s\n"), file_path);
                break;
        }
        
        if (NULL != shell_link) shell_link->Release();
        if (NULL != pf)         pf->Release();
        
        CoUninitialize();
}

// Link with ole32.lib
// ex: SHCreateSystemShortcut(_T("kerker.lnk"), CSIDL_STARTMENU, _T("c:\\windows\\notepad.exe"));
HRESULT SHCreateSystemShortcut(const TCHAR* szLnkFile, int nFolder, const TCHAR* szFile)
{
        CoInitialize(NULL);

        IShellLink* pShellLink = NULL;

        // create proper COM server
        HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
                        reinterpret_cast<LPVOID*>(&pShellLink));

        if(FAILED(hr)) return hr;

        // set attribute
        pShellLink->SetPath(szFile);
        
        IPersistFile* pPF = NULL;
        // get the IPersistFile interface to save
        hr = pShellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID*>(&pPF));

        if(FAILED(hr)) {
                pShellLink->Release();
                return hr;
        }

        TCHAR szPath[MAX_PATH] = {};

        // prepare the name of the shortcut
        SHGetSpecialFolderPath(NULL, szPath, nFolder, FALSE);
        _tprintf(_T("%ls"), szPath);
        if(szPath[lstrlen(szPath) - 1] != '\\')
                lstrcat(szPath, _T("//"));

        lstrcat(szPath, szLnkFile);

        // save to the lnk file (unicode name)
        WCHAR wszLnkFile[MAX_PATH] = {};

        ToWideChar(szPath, wszLnkFile, MAX_PATH);
        _tprintf(_T("shortcut path: %ls\n"), wszLnkFile);
        hr = pPF->Save(wszLnkFile, TRUE);

        // clean
        pPF->Release();
        pShellLink->Release();

        CoUninitialize();
        return hr;
}


//-------------------------------------------------
//                      CH 7
//-------------------------------------------------
bool GetDllVersion(const TCHAR* dll_name, DLLVERSIONINFO2* dll_ver_info)
{
        HMODULE module = LoadLibrary(dll_name);
        bool ret       = false;

        if (module) {
                DLLGETVERSIONPROC DllGetVersionProc = (DLLGETVERSIONPROC)GetProcAddress(module, "DllGetVersion");

                if (DllGetVersionProc) {
                        HRESULT hr = DllGetVersionProc(&dll_ver_info->info1);
                        
                        if (SUCCEEDED(hr)) ret = true;
                }

                FreeLibrary(module);
        }
        
        return ret;
}

void TestShellVersion()
{
        DLLVERSIONINFO2 dll_ver_info = {};
        dll_ver_info.info1.cbSize    = sizeof(dll_ver_info);

        bool ret = GetDllVersion(_T("shell32.dll"), &dll_ver_info);

        if (ret) {
                LONG ver = MAKELONG(dll_ver_info.info1.dwMajorVersion, dll_ver_info.info1.dwMinorVersion);

                printf("major ver=%d minor ver=%d\n", dll_ver_info.info1.dwMajorVersion, dll_ver_info.info1.dwMinorVersion);
                printf("version=%llu %ld\n", dll_ver_info.ullVersion, ver);

                if (dll_ver_info.ullVersion >= MAKEDLLVERULL(4, 71, 0, 0))
                        puts("greater than 4.71");
        }

        printf("4.71=%llu\n", MAKEDLLVERULL(4, 71, 0, 0));
}

void TestHandle(HANDLE handle)
{
        if (handle)
                printf("handle is valid\n");
        else
                printf("handle is null error code=%d\n", GetLastError());
}

void NotifyDataLength()
{
        printf("_WIN32_WINNT=0x%x\n", _WIN32_WINNT);
        printf("sizeof(NOTIFYICONDATA)=%d\n", sizeof(NOTIFYICONDATA));
        printf("NOTIFYICONDATA_V3_SIZE=%d\n", NOTIFYICONDATA_V3_SIZE);
        printf("NOTIFYICONDATA_V2_SIZE=%d\n", NOTIFYICONDATA_V2_SIZE);
        printf("NOTIFYICONDATA_V1_SIZE=%d\n", NOTIFYICONDATA_V1_SIZE);
}

void PrintSystemIconSize()
{
        int icon_width  = GetSystemMetrics(SM_CXICON);
        int icon_height = GetSystemMetrics(SM_CYICON);

        printf("SM_CXICON=%d SM_CYICON=%d\n\n", icon_width, icon_height);
}

// test if we can get the handle of the icon which embedded in the exe file
void TestLoadIcon()
{
        HMODULE module = GetModuleHandle(NULL);
        
        if (module) {
                HICON icon = LoadIcon(module, MAKEINTRESOURCE(IDI_ICON1));
                TestHandle(icon);

                icon = (HICON)LoadImage(module, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_SHARED);
                TestHandle(icon);

                icon = (HICON)LoadImage(NULL, _T("res\\star32.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
                TestHandle(icon);

                icon = (HICON)LoadIcon(NULL, IDI_APPLICATION);
                TestHandle(icon);
        }
}

//void TestTrayIcon(bool is_restart_shell)
//{
//        BOOL ret;
//        MSG  msg = {};
//        UINT icon_uid = 0, icon_message = 0;
//        HWND dialog = GetTrayDialog(IDI_ICON1, &icon_uid, &icon_message);
//
//        if (!dialog) {
//                puts("create tray icon fail!");
//                return;
//        }
//
//        bool result = AddTrayIcon(dialog, IDI_ICON1, icon_uid, icon_message);
//        if (!result) {
//                puts("AddTrayIcon fail!");
//                DestroyWindow(dialog);
//                return;
//        }
//
//        puts("start receive message");
//        
//        int count         = 1;
//        int restart_shell = 0;
//
//        while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
//                printf("message loop thread id=%u\n", GetCurrentThreadId());
//                
//                if (ret == -1) {
//                        PrintError();
//                } else if (!IsDialogMessage(dialog, &msg)) {
//                        puts("get message\n");
//                        TranslateMessage(&msg); 
//                        DispatchMessage(&msg); 
//                }
//                
//                puts("modify icon");
//
//                if (!ModifyTrayIcon(dialog, (count == 1) ? IDI_ICON2 : IDI_ICON1)) {
//                        puts("modify icon fail");
//                        PrintError();
//                }
//
//                count *= -1;
//
//                if (is_restart_shell && ++restart_shell == 3) {
//                        restart_shell = 0;
//                        RestartShell();
//                }
//        }
//
//        puts("exit message loop");
//        Sleep(3000);
//
//        DeleteTrayIcon(dialog);
//
//        puts("delete icon");
//        Sleep(3000);
//}

void TestComObject()
{
        CoInitialize(NULL);

        ISimpleCom* scom = NULL;

        HRESULT result = CoCreateInstance(CLSID_SimpleCom, NULL, CLSCTX_INPROC_SERVER, 
                        IID_ISimpleCom, reinterpret_cast<LPVOID*>(&scom));

        if (FAILED(result)) {
                puts("create ISimpleCom fail!");
                
                if (result == REGDB_E_CLASSNOTREG)
                        puts("REGDB_E_CLASSNOTREG");
                else if (result == CLASS_E_NOAGGREGATION)
                        puts("CLASS_E_NOAGGREGATION");
                else if (result == E_NOINTERFACE)
                        puts("E_NOINTERFACE");
                else if (result == E_POINTER)
                        puts("E_POINTER");
                else if (result == S_OK)
                        puts("S_OK");

                printf("result=%x\n", result);
                return;
        }

        scom->foo(NULL);
        scom->Release();

        CoUninitialize();
}

void TestMonitorDirectory(const TCHAR* path)
{
        HANDLE dir = OpenDirectory(path);

        if (INVALID_HANDLE_VALUE == dir) {
                puts("open dir fail!");
                return;
        }
        
        for (size_t i = 0; i < 0x99999999; i++) {
                char buf[1024] = {};
                FILE_NOTIFY_INFORMATION* notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buf);
                DWORD bytes_returned = 0;
                DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | 
                               FILE_NOTIFY_CHANGE_DIR_NAME |
                               FILE_NOTIFY_CHANGE_ATTRIBUTES |
                               FILE_NOTIFY_CHANGE_SIZE |
                               FILE_NOTIFY_CHANGE_LAST_WRITE |
                               FILE_NOTIFY_CHANGE_LAST_ACCESS |
                               FILE_NOTIFY_CHANGE_CREATION |
                               FILE_NOTIFY_CHANGE_SECURITY;
                
                BOOL result = ReadDirectoryChangesW(dir, notify, sizeof(buf), TRUE, 
                                                    filter, &bytes_returned, NULL, NULL);

                if (result) {
                        switch (notify->Action) {
                        case FILE_ACTION_ADDED:
                                wprintf(L"FILE_ACTION_ADDED %ls\n", notify->FileName);
                                break;
                        case FILE_ACTION_REMOVED:
                                wprintf(L"FILE_ACTION_REMOVED %ls\n", notify->FileName);
                                break;
                        case FILE_ACTION_MODIFIED:
                                wprintf(L"FILE_ACTION_MODIFIED %ls\n", notify->FileName);
                                break;
                        case FILE_ACTION_RENAMED_OLD_NAME:
                                wprintf(L"FILE_ACTION_RENAMED_OLD_NAME %ls\n", notify->FileName);
                                break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                                wprintf(L"FILE_ACTION_RENAMED_NEW_NAME %ls\n", notify->FileName);
                                break;
                        default:
                                ;
                        }
                } else {
                        puts("ReadDirectoryChangesW fail!");
                }
        }
        
        CloseHandle(dir);
}


//-------------------------------------------------
//                      CH 8
//-------------------------------------------------
enum ShellOperator {
        OPEN,
        EXPLORE,
        FIND,
        PRINT
};

// The enum value is called verb for ShellExecute, the ShellExecute lunchs the program
// which registered in registry for the specified file name.
// When user double clicked a file, the explorer calls ShellExecute with open verb.
// 
// Example: 
//      TestShellExecute(OPEN, _T("."));
void TestShellExecute(ShellOperator op, const TCHAR* file_name)
{
        HINSTANCE instance = NULL;
        int res            = 0;

        switch (op) {
        case OPEN:
                instance = ShellExecute(NULL, _T("open"), file_name, NULL, NULL, SW_SHOW);
                break;
        case EXPLORE:
                instance = ShellExecute(NULL, _T("explore"), file_name, NULL, NULL, SW_SHOW);
                break;
        case FIND:
                instance = ShellExecute(NULL, _T("find"), file_name, NULL, NULL, SW_SHOW);
                break;
        case PRINT:
                instance = ShellExecute(NULL, _T("print"), file_name, NULL, NULL, SW_SHOW);
                break;
        default:
                ;
        }

        res = reinterpret_cast<int>(instance);
        if (SE_ERR_NOASSOC == res)
                puts("no application associate with the file");

        _tprintf(_T("Result of ShellExecute=%x\n"), res);
}

void TestFindExecutable(const TCHAR* file_name)
{
        FILE* file                 = _tfopen(file_name, _T("w"));
        TCHAR executable[MAX_PATH] = {};

        fclose(file);
        HINSTANCE res = FindExecutable(file_name, NULL, executable);
        
        if (SE_ERR_NOASSOC == reinterpret_cast<int>(res))
                _tprintf(_T("file = %ls no file handler\n"), file_name);
        else
                _tprintf(_T("file    = %ls\nhandler = %ls\n"), file_name, executable);
        
        _tremove(file_name);
}

void TestShellExecuteEx(ShellOperator op)
{
        LPITEMIDLIST pidl;
        SHELLEXECUTEINFO sei = {};
        
        switch (op) {
        case OPEN:
                SHGetSpecialFolderLocation(NULL, CSIDL_PRINTERS, &pidl);
        
                sei.cbSize   = sizeof(sei);
                sei.nShow    = SW_SHOW;
                sei.lpIDList = pidl;
                sei.fMask    = SEE_MASK_INVOKEIDLIST;
                sei.lpVerb   = _T("open");
        
                break;
        default:
                ;
        }
        
        ShellExecuteEx(&sei);
}

void ShowFileProperties(const TCHAR* file_name)
{
        SHELLEXECUTEINFO sei = {};

        sei.cbSize   = sizeof(sei);
        sei.nShow    = SW_SHOW;
        sei.lpFile   = file_name;
        sei.lpVerb   = _T("properties");
        sei.fMask    = SEE_MASK_INVOKEIDLIST;   // must set SEE_MASK_INVOKEIDLIST to use dynamic verb
        
        ShellExecuteEx(&sei);
        
        puts("please enter one character");

        // prevent the program exit, else the properties dialog will exit right before the program exit.
        getchar();
}


//-------------------------------------------------
//                      CH 9
//-------------------------------------------------
void RestartShell()
{
        HWND window = GetShellWindow();
        PostMessage(window, WM_QUIT, 0, 0);
        ShellExecute(NULL, NULL, _T("explorer.exe"), NULL, NULL, SW_SHOW);
}

void PrintTaskbarStatus()
{
        APPBARDATA bar_data = {};

        bar_data.cbSize  = sizeof(bar_data);
        UINT_PTR res     = SHAppBarMessage(ABM_GETSTATE, &bar_data);
        
        if (res & ABS_ALWAYSONTOP)
                puts("taskbar always on top!");
        else
                puts("taskbar not always on top!");

        if (res & ABS_AUTOHIDE)
                puts("taskbar autohide");
        else
                puts("taskbar not autohide!");
        
        memset(&bar_data, 0, sizeof(bar_data));
        bar_data.cbSize = sizeof(bar_data);
        SHAppBarMessage(ABM_QUERYPOS, &bar_data);

        switch (bar_data.uEdge) {
        case ABE_BOTTOM:
                puts("aligned at bottom!");
                break;
        case ABE_TOP:
                puts("aligned at top!");
                break;
        case ABE_LEFT:
                puts("aligned at left!");
                break;
        case ABE_RIGHT:
                puts("aligned at right!");
                break;
        default:
                ;
        }
        
        memset(&bar_data, 0, sizeof(bar_data));
        bar_data.cbSize = sizeof(bar_data);
        SHAppBarMessage(ABM_GETTASKBARPOS, &bar_data);

        printf("taskbar left=%d right=%d top=%d bottom=%d uedge=%u\n", 
                        bar_data.rc.left, bar_data.rc.right, bar_data.rc.top, bar_data.rc.bottom, bar_data.uEdge);
}

void SetTaskbarAutohide()
{
        APPBARDATA bar_data = {};

        bar_data.cbSize = sizeof(bar_data);
        bar_data.lParam = ABS_AUTOHIDE | ABS_ALWAYSONTOP;
        SHAppBarMessage(ABM_SETSTATE, &bar_data);
}


//-------------------------------------------------
//                      CH 10
//-------------------------------------------------
bool GetFileVersion(const TCHAR* file_name, WORD version[], size_t ver_len)
{
        assert(version != NULL && ver_len != 0);

        DWORD tmp = 0;
        DWORD len = GetFileVersionInfoSize(file_name, &tmp);

        if (0 == len) return false;
        
        char* buf                   = new char[len];
        VS_FIXEDFILEINFO* file_info = NULL;
        UINT buf_len                = 0;

        if (!GetFileVersionInfo(file_name, NULL, len, buf))
                goto error_out;
        
        if (!VerQueryValue(buf, _T("\\"), reinterpret_cast<void**>(&file_info), &buf_len))
                goto error_out;

        WORD tmp_buf[] = {
                HIWORD(file_info->dwFileVersionMS),
                LOWORD(file_info->dwFileVersionMS),
                HIWORD(file_info->dwFileVersionLS),
                LOWORD(file_info->dwFileVersionLS)
        };
        
        for (size_t i = 0; i < ver_len && i < 4; i++)
                version[i] = tmp_buf[i];

        delete [] buf;
        return true;

error_out:
        delete [] buf;
        return false;
}

void AskForEmptyRecycleBin()
{
        TCHAR drive_string[64] = {};
        DWORD len = GetLogicalDriveStrings(sizeof(drive_string) - 1, drive_string);

        for (DWORD i = 0; i < len; i += 4) {
                SHQUERYRBINFO info = {sizeof(info)};

                if(DRIVE_FIXED == GetDriveType(&drive_string[i])) {
                        SHQueryRecycleBin(&drive_string[i], &info);

                        _tprintf(_T("Recycle Bin of %ls\n"), &drive_string[i]);
                        printf("size=%lld bytes num items=%lld\n\n", info.i64Size, info.i64NumItems);
                        SHEmptyRecycleBin(NULL, &drive_string[i], 0);
                }
        }

}

void PrintOsVersion()
{
        OSVERSIONINFOEX os_info = {sizeof(os_info)};

        if (GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&os_info))) {
                printf("major Version=%u\nminor Version=%u\nbuild number=%u\nplatform id=%u\n\n",
                                os_info.dwMajorVersion, os_info.dwMinorVersion, 
                                os_info.dwBuildNumber, os_info.dwPlatformId);
                
                _tprintf(_T("szCSDVersion=%ls\n\n"), os_info.szCSDVersion);
                printf("service pack\nmajor=%u\nminor=%u\n\n", os_info.wServicePackMajor, os_info.wServicePackMinor);

                switch (os_info.wProductType) {
                case VER_NT_DOMAIN_CONTROLLER:
                        puts("product type=VER_NT_DOMAIN_CONTROLLER");
                        break;
                case VER_NT_SERVER:
                        puts("product type=VER_NT_SERVER");
                        break;
                case VER_NT_WORKSTATION:
                        puts("product type=VER_NT_WORKSTATION");
                        break;
                default:
                        ;
                }
        } else {
                puts("Get Os information fail!");
                PopupError();
        }
}


//-------------------------------------------------
//                      CH 11
//-------------------------------------------------
void OpenRecycleBin()
{
        // Use CLSID to open special folder from command line
        system("explorer ::{645FF040-5081-101B-9F08-00AA002F954E}");
}

void OpenPrintersFolder()
{
        // Can use \ to treat CLSID as a real folder
        system("explorer ::{20d04fe0-3aea-1069-a2d8-08002b30309d}\\::{2227a280-3aea-1069-a2de-08002b30309d}");
}

void TestRundll32()
{
        // Must use the function name after name mangling.
        // If don't want to name mangling, then we should use def file to build the dll.
        system("Rundll32 my_dll,?Rundll32Test@@YGXPAUHWND__@@PAUHINSTANCE__@@PADH@Z 中文");
}

void OpenDesktopControlPanel()
{
        // We can use the Control_RunDLL function which exposed by shell32.dll to
        // open the desktop control panel.
        // The last argument 0 means to open the control panel with the first tab
        system("Rundll32 shell32,Control_RunDLL desk.cpl,,0");
}

// Example: 
//      GetOpenFileNameByFilter(_T("All\0*.*\0Image Files\0*.jpg;*.png;\0"), NULL, _T("kerker.jpg"));
void GetOpenFileNameByFilter(const TCHAR* filter, const TCHAR* init_dir, TCHAR* init_file_name)
{
        OPENFILENAME open_file_name      = {};
        TCHAR        file_name[MAX_PATH] = {};

        if (init_file_name)
                _tcsncat(file_name, init_file_name, _countof(file_name) - 1);

        open_file_name.lStructSize     = sizeof(open_file_name);
        open_file_name.lpstrFilter     = filter;
        open_file_name.nMaxFile        = MAX_PATH;
        open_file_name.lpstrInitialDir = init_dir;
        open_file_name.lpstrFile       = file_name;
        open_file_name.nFilterIndex    = 1;
        open_file_name.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        
        if (GetOpenFileName(&open_file_name)) {
               _tprintf(_T("selected file: %ls\n"), open_file_name.lpstrFile);
        } else {
                // Check if user close the dialog
                if (0 != CommDlgExtendedError())
                        PopupError();
        }
}


//-------------------------------------------------
//                      CH 13
//-------------------------------------------------
bool TestCopyToClipboard(const TCHAR* text)
{
        HGLOBAL buf_handle = NULL;
        TCHAR* buf         = NULL;
        int text_len       = lstrlen(text);

        if (!OpenClipboard(NULL))
                goto error_out;
        
        if (!EmptyClipboard())
                goto release_res;
        
        // Clipboard needs the data stored in heap
        buf_handle = GlobalAlloc(GMEM_MOVEABLE, (text_len + 1) * sizeof(TCHAR));
        buf        = static_cast<TCHAR*>(GlobalLock(buf_handle));
        
        memcpy(buf, text, text_len * sizeof(TCHAR));
        buf[text_len] = _T('\0');
        GlobalUnlock(buf_handle);

        if (!SetClipboardData(GetClipboardTextFlag(), buf_handle))
                goto release_res;

        CloseClipboard();
        return true;

release_res:
        CloseClipboard();

error_out:
        PrintError();
        return false;
}

// The buf_len should be the length of the char buf, not size.
// For example the buf_len of TCHAR buf[5] should be 5.
bool ReadFromClipboard(TCHAR* buf, size_t buf_len)
{
        if (!OpenClipboard(NULL)) {
                PrintError();
                return false;
        }

        HANDLE data = GetClipboardData(GetClipboardTextFlag());
        
        bool ret = false;
        if (data) {
                TCHAR* text     = static_cast<TCHAR*>(GlobalLock(data));
                size_t text_len = lstrlen(text);
                size_t min_len  = text_len < buf_len ? text_len : buf_len - 1;
                
                memcpy(buf, text, min_len * sizeof(TCHAR));
                buf[min_len] = _T('\0');
                GlobalUnlock(data);
                ret = true;
        } else {
                PrintError();
        }

        CloseClipboard();
        return ret;
}

// Link with Advapi32.lib
// Example:
//      const TCHAR* key = _T("System\\CurrentControlSet\\Control");
//      GetNthValue(HKEY_LOCAL_MACHINE, key, 0, buf, &buf_len);
bool GetNthKey(HKEY root, const TCHAR* key_path, int index, TCHAR* buf, DWORD* buf_len)
{
        HKEY key_handle = NULL;

        if (ERROR_SUCCESS != RegOpenKeyEx(root, key_path, 0, KEY_READ, &key_handle))
                goto error_out;
        
        if (ERROR_SUCCESS != SHEnumKeyEx(key_handle, index, buf, buf_len))
                goto release_res;
        
        RegCloseKey(key_handle);
        return true;

release_res:
        RegCloseKey(key_handle);

error_out:
        PrintError();
        return false;
}

bool GetNthValue(HKEY root, const TCHAR* value_path, int index, TCHAR* buf, DWORD* buf_len)
{
        HKEY value_handle = NULL;

        if (ERROR_SUCCESS != RegOpenKeyEx(root, value_path, 0, KEY_ALL_ACCESS, &value_handle))
                goto error_out;

        if (ERROR_SUCCESS != SHEnumValue(value_handle, index, buf, buf_len, NULL, NULL, NULL))
                goto release_res;

        RegCloseKey(value_handle);
        return true;

release_res:
        RegCloseKey(value_handle);

error_out:
        PrintError();
        return false;
}

//-------------------------------------------------
//                      CH 14
//-------------------------------------------------
bool TestAnotherInstanceRunning()
{
        TCHAR mutex_name[] = _T("{95857007-C976-4a1d-BDD1-08C8450A8495}");
        HANDLE mutex       = CreateMutex(NULL, FALSE, mutex_name);

        if (!mutex) {
                PrintError();
                return false;
        }

        if (ERROR_ALREADY_EXISTS == GetLastError()) {
                ReleaseMutex(mutex);
                return true;
        } else {
                return false;
        }
}


//-------------------------------------------------
//                      CH 15
//-------------------------------------------------
BOOL CALLBACK GetImportFunctions(IMAGEHLP_STATUS_REASON Reason,
                                 PCSTR,
                                 PCSTR DllName,
                                 ULONG_PTR,
                                 ULONG_PTR Parameter)
{
        if (BindImportProcedure == Reason) {
                CHAR* func_name = reinterpret_cast<CHAR*>(Parameter);

                printf("dll=%s func=%s\n", DllName, func_name);
        }

        return TRUE;
}

BOOL CALLBACK GetImportModules(IMAGEHLP_STATUS_REASON Reason,
                               PCSTR,
                               PCSTR DllName,
                               ULONG_PTR,
                               ULONG_PTR)
{
        if (BindImportModule == Reason || BindImportModuleFailed == Reason)
                printf("%s\n", DllName);

        return TRUE;
}

BOOL PrintImportFunctions(const CHAR* file_name)
{
        const CHAR* path = "C:\\WINDOWS\\system32";

        return BindImageEx(BIND_NO_BOUND_IMPORTS | BIND_NO_UPDATE, file_name, 
                           path, path, GetImportFunctions);
}

BOOL PrintImportModules(const CHAR* file_name)
{
        const CHAR* path = "C:\\WINDOWS\\system32";

        return BindImageEx(BIND_NO_BOUND_IMPORTS | BIND_NO_UPDATE, file_name, 
                           path, path, GetImportModules);
}

void TestPrintImportFunctions()
{
        char buf[512] = {};

        while (EOF != scanf("%s", buf)) {
                PrintImportFunctions(buf);
                PrintError();
                puts("");
        }
}

void TestPrintImportModules()
{
        char buf[MAX_PATH] = {};

        while (fgets(buf, MAX_PATH, stdin)) {
                PrintImportModules(buf);
                PrintError();
                puts("");
        }
}


//-------------------------------------------------
//          Other functions for testing
//-------------------------------------------------
void printArgs(int argc, char* argv[])
{
        for (int i = 0; i < argc; i++)
                printf("%s\n", argv[i]);

        if (argc < 2)
                return;

        FILE* f = fopen(argv[1], "r");
        if (f) {
                char buf[64] = {};

                fgets(buf, sizeof(buf), f);
                printf("%s\n", buf);
        } else {
                puts("open file fail!");
        }

        getchar();               
}


class A {
public:
        static A* getInstance() {
                static A* inst = 0;
                
                if (inst == 0)
                        inst = new A;

                return inst;
        }

        void print(int i) {
                printf("%d\n", i);
        }
};

int main(int argc, char** argv)
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
        int ch = 0;

        while ((ch = getopt(argc, argv, "a:b")) != -1) {
                switch (ch) {
                case 'a':
                        printf("-a and %s\n", optarg);
                        break;
                case 'b':
                        puts("-b");
                        break;
                default:
                        puts("kerker");
                        break;
                }
        }
        return 0;
}
