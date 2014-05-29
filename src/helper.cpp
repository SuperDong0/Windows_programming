#define _CRT_SECURE_NO_WARNINGS

#include "helper.h"
#include <cstdio>
#include <cassert>
#include <Shlwapi.h>
#include <Shlobj.h>

namespace {


}       // End of unnamed namespace

//-------------------------------------------------
//                 Error handling 
//-------------------------------------------------
void PrintErrorWith(const WCHAR* custom_message)
{
        DWORD error    = GetLastError();
        WCHAR* msg_buf = NULL;

        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<WCHAR*>(&msg_buf),
                       0, NULL);
        
        printf("%ls\nError %u: %ls\n", custom_message, error, msg_buf);

        LocalFree(msg_buf);
}

void PrintErrorWith(const CHAR* custom_message)
{
        DWORD error   = GetLastError();
        CHAR* msg_buf = NULL;

        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<CHAR*>(&msg_buf),
                       0, NULL);
        
        printf("%s\nError %u: %s\n", custom_message, error, msg_buf);

        LocalFree(msg_buf);
}

void PrintError()
{
        PrintErrorWith("");
}

void PopupErrorWith(const WCHAR* custom_message)
{
        DWORD error    = GetLastError();
        WCHAR* msg_buf = NULL;

        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<WCHAR*>(&msg_buf),
                       0, NULL);

        WCHAR tmp_buf[512] = {};

        wcsncat(tmp_buf, custom_message, _countof(tmp_buf) - 2);

        int msg_len      = wcslen(tmp_buf);
        tmp_buf[msg_len] = L'\n';
        
        // Now the length of string in tmp_buf is msg_len + 1
        // At most we can append _countof(tmp_buf) - (msg_len + 1) - 1
        // Minus the last one char because strncat will always append
        // null byte to the dest buf
        wcsncat(tmp_buf, msg_buf, _countof(tmp_buf) - msg_len - 2);

        MessageBoxW(NULL, tmp_buf, L"Error", MB_OK);
        LocalFree(msg_buf);
}

void PopupErrorWith(const CHAR* custom_message)
{
        DWORD error   = GetLastError();
        CHAR* msg_buf = NULL;

        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<CHAR*>(&msg_buf),
                       0, NULL);

        CHAR tmp_buf[512] = {};

        strncat(tmp_buf, custom_message, _countof(tmp_buf) - 2);

        int msg_len      = strlen(tmp_buf);
        tmp_buf[msg_len] = '\n';
        
        // Now the length of string in tmp_buf is msg_len + 1
        // At most we can append _countof(tmp_buf) - (msg_len + 1) - 1
        // Minus the last one char because strncat will always append
        // null byte to the dest buf
        strncat(tmp_buf, msg_buf, _countof(tmp_buf) - msg_len - 2);

        MessageBoxA(NULL, tmp_buf, "Error", MB_OK);
        LocalFree(msg_buf);
}

void PopupError()
{
        PopupErrorWith("");
}


//-------------------------------------------------
//                     Log
//-------------------------------------------------
void RedirectConsoleOutputToFile()
{
        freopen("stdout.txt", "w", stdout);
        freopen("stderr.txt", "w", stderr);
}

// No need to close the file which created by RedirectConsoleOutputToFile
// because freopen will close the file itself
void RecoverConsoleOutput()
{
        freopen("CON", "w", stdout);
        freopen("CON", "w", stderr);
}


//-------------------------------------------------
//                      COM
//-------------------------------------------------
HRESULT StringFromCLSID2(const CLSID& clsid, WCHAR* buf, size_t buf_len)
{
        TCHAR* tmp  = NULL;
        HRESULT ret = StringFromCLSID(clsid, &tmp);

        if (S_OK == ret) {
                size_t tmp_len = lstrlen(tmp);
                size_t len     = (tmp_len < buf_len) ? tmp_len : buf_len - 1;
        
                _tcsncat(buf, tmp, len);
                CoTaskMemFree(tmp);
        }
        return ret;
}

HRESULT ProgIDFromCLSID2(const CLSID& clsid, WCHAR* buf, size_t buf_len)
{
        LPOLESTR prog_id = NULL;
        HRESULT result   = ProgIDFromCLSID(clsid, &prog_id);

        wcsncat(buf, prog_id, buf_len - 1);
        CoTaskMemFree(prog_id);

        return result;
}


//-------------------------------------------------
//                    Others
//-------------------------------------------------
// Link with Shlwapi.lib
HRESULT FindExecutable2(const TCHAR* extension, TCHAR* exe_path_buf, DWORD buf_size)
{
        return AssocQueryString(0, ASSOCSTR_EXECUTABLE, extension, _T("open"), 
                                exe_path_buf, &buf_size); 
}

// Return 0 means use MultiByteToWideChar and function fail
// Return a negative value means use swprintf and function fail
int ToWideChar(const TCHAR* source, WCHAR* dest, int dest_len)
{
#ifdef UNICODE
        return swprintf(dest, dest_len - 1, L"%ls", source);
#else
        return MultiByteToWideChar(CP_ACP, 0, source, -1, dest, dest_len);
#endif
}

UINT GetClipboardTextFlag()
{
#ifdef UNICODE
        return CF_UNICODETEXT;
#else
        return CF_TEXT;
#endif
}

HANDLE OpenDirectory(const TCHAR* path)
{
        return CreateFile(path, 
                          GENERIC_READ | FILE_LIST_DIRECTORY,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS,
                          NULL);
}

HANDLE OpenFile2(const TCHAR* path)
{
        return CreateFile(path,
                          GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
}

FARPROC GetProcAddress2(char* lib_name, char* func_name)
{
        return GetProcAddress(GetModuleHandleA(lib_name), func_name);
}

const TCHAR* GetFileTypeString(DWORD type)
{
        // The string literal will be stored in global data section,
        // so we can return the address directly.
        switch (type) {
        case 0x0002:
                return _T("FILE_TYPE_CHAR");
        case 0x0001:
                return _T("FILE_TYPE_DISK");
        case 0x0003:
                return _T("FILE_TYPE_PIPE");
        case 0x8000:
                return _T("FILE_TYPE_REMOTE");
        case 0x0000:
                return _T("FILE_TYPE_UNKNOWN");
        default:
                return _T("");
        }
}

// Example: 
//      GetOpenFileNameByFilter(_T("All\0*.*\0Image Files\0*.jpg;*.png;\0"), NULL, _T("test.jpg"));
bool GetOpenFileNameByFilter(HWND window,
                             const TCHAR* filter,
                             const TCHAR* init_dir,
                             const TCHAR* init_file_name,
                             TCHAR* buf,
                             size_t buf_len)
{
        OPENFILENAME open_file    = {};
        TCHAR file_name[MAX_PATH] = {};

        if (init_file_name)
                _tcsncat(file_name, init_file_name, _countof(file_name) - 1);
        
        open_file.hwndOwner       = window;
        open_file.lStructSize     = sizeof(open_file);
        open_file.lpstrFilter     = filter;
        open_file.nMaxFile        = MAX_PATH;
        open_file.lpstrInitialDir = init_dir;
        open_file.lpstrFile       = file_name;
        open_file.nFilterIndex    = 1;
        open_file.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        
        if (GetOpenFileName(&open_file)) {
                _tcsncat(buf, open_file.lpstrFile, buf_len - 1);
                return true;
        } else {
                // Check if user close the dialog
                if (0 != CommDlgExtendedError())
                        PopupError();
                
                return false;
        }
}

bool DrivePathToDevicePath(const TCHAR* drive_path, TCHAR* buf, size_t buf_len)
{
        TCHAR target[MAX_PATH] = {};
        TCHAR device[3]        = {drive_path[0], drive_path[1]};

        DWORD result = QueryDosDevice(device, target, _countof(target));

        if (result) {
                _sntprintf(buf, buf_len - 1, _T("%ls\\%ls"), target, &drive_path[3]);
                return true;
        } else {
                PrintErrorWith(_T("QueryDosDevice fail!"));
                return false;
        }
}

bool DevicePathToDrivePath(const TCHAR* device_path, TCHAR* buf, size_t buf_len)
{
        static TCHAR drives[26] = {};
        static int num_drives   = 0;
        
        // Get the logical drive infomation only one time
        // to save the performance.
        if (0 == num_drives) {
                TCHAR buf[1024] = {};

                DWORD result = GetLogicalDriveStrings(_countof(buf) - 1, buf);
                if (result == 0) {
                        PrintError();
                        return false;
                }

                for (DWORD i = 0; i < result; i++) {
                        if (_T('A') <= buf[i] && _T('Z') >= buf[i])
                                drives[num_drives++] = buf[i];
                }
        }

        for (int i = 0; i < num_drives; i++) {
                TCHAR drive[3]         = {drives[i], _T(':')};
                TCHAR target[MAX_PATH] = {};

                if (QueryDosDevice(drive, target, _countof(target))) {
                        // The return value of QueryDosDevice is a wrong number
                        // in XP SP3, so we need to check the actual string
                        // length by ourself.
                        int len = lstrlen(target);

                        if (0 == _tcsnccmp(target, device_path, len)) {
                                _sntprintf(buf,
                                           buf_len - 1,
                                           _T("%lc:%ls"),
                                           drives[i],
                                           &device_path[len]);
                                return true;
                        }
                } else {
                        PrintErrorWith(_T("QueryDosDevice fail!"));
                        return false;
                }
        }
 
        return false;
}

bool SetPrivilege(HANDLE hToken,          // access token handle
                  LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
                  bool bEnablePrivilege)  // to enable or disable privilege
{
        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (!LookupPrivilegeValue(NULL,            // lookup privilege on local system
                                  lpszPrivilege,   // privilege to lookup 
                                  &luid)) {        // receives LUID of privilege
                PrintErrorWith(_T("LookupPrivilegeValue error: %u\n")); 
                return false; 
        }

        tp.PrivilegeCount     = 1;
        tp.Privileges[0].Luid = luid;
        if (bEnablePrivilege)
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        else
                tp.Privileges[0].Attributes = 0;

        // Enable the privilege or disable all privileges.

        if (!AdjustTokenPrivileges(hToken, 
                                   FALSE, 
                                   &tp, 
                                   sizeof(TOKEN_PRIVILEGES), 
                                   NULL, 
                                   NULL)) { 
                PrintErrorWith(_T("AdjustTokenPrivileges error: %u\n")); 
                return false; 
        } 

        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
                printf("The token does not have the specified privilege.\n");
                return false;
        } 

        return true;
}

bool AnotherInstanceRunning()
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

// ex: CreateSystemShortcut(_T("kerker.lnk"), CSIDL_STARTMENU, _T("c:\\windows\\notepad.exe"));
HRESULT CreateSystemShortcut(const TCHAR* shortcut_name,
                             int folder_id,
                             const TCHAR* target_path)
{
        CoInitialize(NULL);

        IShellLink* shell_link     = NULL;
        IPersistFile* persist_file = NULL;

        // Get COM objects for later use
        HRESULT hr = CoCreateInstance(CLSID_ShellLink,
                                      NULL, 
                                      CLSCTX_INPROC_SERVER, 
                                      IID_IShellLink,
                                      reinterpret_cast<LPVOID*>(&shell_link));
        if(FAILED(hr)) return hr;

        shell_link->SetPath(target_path);
        hr = shell_link->QueryInterface(IID_IPersistFile,
                                        reinterpret_cast<LPVOID*>(&persist_file));
        if(FAILED(hr)) {
                shell_link->Release();
                return hr;
        }
        
        // Get special folder path and save shortcut to it.
        TCHAR folder_path[MAX_PATH]  = {};
        WCHAR folder_path2[MAX_PATH] = {};

        SHGetSpecialFolderPath(NULL, folder_path, folder_id, FALSE);
        if(folder_path[lstrlen(folder_path) - 1] != '\\')
                lstrcat(folder_path, _T("//"));
        
        _tcsncat(folder_path, shortcut_name, _countof(folder_path) - lstrlen(folder_path) - 1);
        _tcsncat(folder_path, _T(".lnk"), _countof(folder_path) - lstrlen(folder_path) - 1);

        ToWideChar(folder_path, folder_path2, MAX_PATH);
        hr = persist_file->Save(folder_path2, TRUE);

        persist_file->Release();
        shell_link->Release();

        CoUninitialize();
        return hr;
}

// This function can compute the LEFT-UPPER point of the window
// which align to the taskbar
void GetPointAlignTaskbar(POINT* point, LONG width, LONG height)
{
        APPBARDATA bar_data = {sizeof(bar_data)};

        SHAppBarMessage(ABM_GETTASKBARPOS, &bar_data);

        switch(bar_data.uEdge) {
        case ABE_BOTTOM:
                point->x = bar_data.rc.right - width;
                point->y = bar_data.rc.top - height;
                break;
        case ABE_TOP:
                point->x = bar_data.rc.right - width;
                point->y = bar_data.rc.bottom;
                break;
        case ABE_LEFT:
                point->x = bar_data.rc.right;
                point->y = bar_data.rc.bottom - height;
                break;
        case ABE_RIGHT:
                point->x = bar_data.rc.left - width;
                point->y = bar_data.rc.bottom - height;
                break;
        default:
                ;
        }
}

bool CopyToClipboard(const TCHAR* text)
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

void GetModulePath(HMODULE module, TCHAR* buf, size_t buf_len)
{
        GetModuleFileName(module, buf, buf_len);
        PathRemoveFileSpec(buf);
}

bool IsPathExucutable(const TCHAR* file_path)
{
        assert(file_path != NULL && "file_path cannot be NULL!");
        
        if (!PathFileExists(file_path))
                return false;
        
        if (PathIsDirectory(file_path))
                return false;

        return true;
}

// The function will create a temp file in the system temp directory,
// don't forget to remove the file when you don't need the temp file.
void GetTempFilePath(TCHAR* buf, DWORD buf_len, const TCHAR* prefix)
{
        TCHAR name[MAX_PATH]  = {};
        TCHAR path[MAX_PATH] = {};

        GetTempPath(_countof(path), path);
        GetTempFileName(path, prefix, 0, name);

        _tcsncat(buf, name, buf_len - 1);
}

FileHandle::FileHandle(const TCHAR* file_path, const TCHAR* mode)
        : f_(NULL)
{
        f_ = _tfopen(file_path, mode);
}

FileHandle::~FileHandle()
{
        if (f_)
                fclose(f_);
}
