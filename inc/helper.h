#ifndef HELPER_H
#define HELPER_H

#include <tchar.h>
#include <windows.h>

void PopupError();
void PrintError();
void PopupErrorWith(const WCHAR* custom_message);
void PopupErrorWith(const CHAR* custom_message);
void PrintErrorWith(const WCHAR* custom_message);
void PrintErrorWith(const CHAR* custom_message);
void PopupErrorWithCustomMessage(const TCHAR* custom_message);
void PrintErrorWithCustomMessage(const TCHAR* custom_message);
void RedirectConsoleOutputToFile();
void RecoverConsoleOutput();
HRESULT FindExecutableEx(const TCHAR* extension, TCHAR* exe_path_buf, DWORD buf_size);
int ToWideChar(const TCHAR* source, WCHAR* dest, int dest_len);
UINT GetClipboardTextFlag();
HRESULT StringFromCLSIDEx(const CLSID& clsid, WCHAR* buf, size_t buf_len);
HANDLE OpenDirectory(const TCHAR* path);
HANDLE OpenFile2(const TCHAR* path);
FARPROC GetProcAddress2(char* lib_name, char* func_name);
const TCHAR* GetFileTypeString(DWORD type);
bool GetOpenFileNameByFilter(HWND window,
                             const TCHAR* filter,
                             const TCHAR* init_dir,
                             const TCHAR* init_file_name,
                             TCHAR* buf,
                             size_t buf_len);

bool DrivePathToDevicePath(const TCHAR* drive_path, TCHAR* buf, size_t buf_len);
bool DevicePathToDrivePath(const TCHAR* device_path, TCHAR* buf, size_t buf_len);
bool SetPrivilege(HANDLE hToken,
                  LPCTSTR lpszPrivilege,
                  bool bEnablePrivilege);
bool AnotherInstanceRunning();
HRESULT CreateSystemShortcut(const TCHAR* shortcut_name,
                             int folder_id,
                             const TCHAR* target_path);

void GetPointAlignTaskbar(POINT* point, LONG width, LONG height);
bool CopyToClipboard(const TCHAR* text);
void GetModulePath(HMODULE module, TCHAR* buf, size_t buf_len);
bool IsPathExucutable(const TCHAR* file_path);
void GetTempFilePath(TCHAR* buf, DWORD buf_len, const TCHAR* prefix);

class FileHandle {
public:
        FileHandle(const TCHAR* file_path, const TCHAR* mode);
        ~FileHandle();

        FILE* f_;
};

#endif  // End of header guard
