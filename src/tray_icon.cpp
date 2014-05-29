#include "tray_icon.h"
#include "resource.h"
#include "helper.h"
#include <cstdio>
#include <cassert>
#include <map>
#include <tchar.h>
#include <Shlwapi.h>

using std::map;

namespace {

HINSTANCE  g_instance   = NULL;
map<HWND, UINT> g_icon_uid_list;

DWORD GetSizeByShellDll()
{
        static DWORD size = 0;

        if (size != 0) return size;

        HMODULE module = LoadLibrary(_T("shell32.dll"));

        if (module) {
                DLLGETVERSIONPROC DllGetVersionProc = (DLLGETVERSIONPROC)GetProcAddress(module, "DllGetVersion");
                
                DLLVERSIONINFO2 dll_ver_info = {};
                dll_ver_info.info1.cbSize    = sizeof(dll_ver_info);

                if (DllGetVersionProc) {
                        HRESULT hr = DllGetVersionProc(&dll_ver_info.info1);

                        if (SUCCEEDED(hr)) {
                                if (dll_ver_info.ullVersion >= MAKEDLLVERULL(6, 0, 6, 0))
                                        size = sizeof(NOTIFYICONDATA);
                                else if (dll_ver_info.ullVersion >= MAKEDLLVERULL(6, 0, 0, 0))
                                        size = NOTIFYICONDATA_V3_SIZE;
                                else if (dll_ver_info.ullVersion >= MAKEDLLVERULL(5, 0, 0, 0))
                                        size = NOTIFYICONDATA_V2_SIZE;
                                else
                                        size = NOTIFYICONDATA_V2_SIZE;
                        }
                }

                FreeLibrary(module);
        }

        return size;
}

bool FillSize(NOTIFYICONDATA* icon_data)
{
        DWORD size = GetSizeByShellDll();

        if (0 == size) {
                puts("get NOTIFYICONDATA size fail!");
                return false;
        }

        icon_data->cbSize = size;
        return true;
}

bool FillIcon(NOTIFYICONDATA* icon_data, WORD icon_id)
{
        // If use LR_SHARED flag, then we don't need to close the icon handle
        HICON icon = (HICON)LoadImage(g_instance, MAKEINTRESOURCE(icon_id), IMAGE_ICON, 0, 0, LR_SHARED);
        if (!icon) {
                printf("load icon id=%u fail!\n", icon_id);
                return false;
        }

        icon_data->hIcon = icon;
        return true;
}

}       // End of unnamed namespace

bool AddTrayIcon(HWND window, 
                 WORD icon_id,
                 UINT icon_uid, 
                 UINT icon_message,
                 const TCHAR* tip,
                 DWORD timeout)
{
        assert(window != NULL && "the window handle should not be NULL!");
        
        if (!g_instance)
                g_instance = GetModuleHandle(NULL);
        
        NOTIFYICONDATA icon_data = {};
       
        if (!FillIcon(&icon_data, icon_id)) return false;
        if (!FillSize(&icon_data))          return false;

        icon_data.hWnd             = window;
        icon_data.uID              = icon_uid;
        icon_data.uFlags           = NIF_MESSAGE | NIF_TIP | NIF_ICON;
        icon_data.uCallbackMessage = icon_message;
        
        // Not use StringCchCopy because the API supported only on the XP SP2 and later version
        if (tip)
                lstrcpyn(icon_data.szTip, tip, 64);
        
        DWORD time_passed = 0;
        while (time_passed <= timeout) {
                if (Shell_NotifyIcon(NIM_ADD, &icon_data)) {
                        g_icon_uid_list[window] = icon_uid;
        
                        return true;
                } else {
                        PrintErrorWith(_T("Shell_NotifyIcon add icon fail!"));
                        Sleep(200);     // Sleep 0.2 second
                        time_passed += 200;
                }
        }
        return false;
}

bool DeleteTrayIcon(HWND window, DWORD timeout)
{
        assert(window != NULL && "the window handle should not be NULL!");

        NOTIFYICONDATA icon_data = {};
       
        if (!FillSize(&icon_data)) return false;
        
        icon_data.hWnd = window;
        icon_data.uID  = g_icon_uid_list.find(window)->second;
        
        DWORD time_passed = 0;
        while (time_passed <= timeout) {
                if (Shell_NotifyIcon(NIM_DELETE, &icon_data)) {
                        g_icon_uid_list.erase(window);
        
                        return true;
                } else {
                        PrintErrorWith(_T("Shell_NotifyIcon delete icon fail!"));
                        Sleep(200);
                        time_passed += 200;
                }
        }
        return false;
}

bool ModifyTrayIcon(HWND window, WORD icon_id, DWORD timeout)
{
        assert(window != NULL && "the window handle should not be NULL!");

        NOTIFYICONDATA icon_data = {};
        
        if (!FillIcon(&icon_data, icon_id)) return false;
        if (!FillSize(&icon_data))          return false;

        icon_data.hWnd   = window;
        icon_data.uID    = g_icon_uid_list.find(window)->second;
        icon_data.uFlags = NIF_ICON;
        
        DWORD time_passed = 0;
        while (time_passed <= timeout) {
                if (Shell_NotifyIcon(NIM_MODIFY, &icon_data)) {
                        return true;
                } else {
                        PrintErrorWith(_T("Shell_NotifyIcon modify icon fail!"));
                        Sleep(200);
                        time_passed += 200;
                }
        }
        return false;
}
