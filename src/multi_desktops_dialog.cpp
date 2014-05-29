#define _CRT_SECURE_NO_WARNINGS

#include "simple_tray_icon_menu.h"
#include "resource.h"
#include "tray_icon.h"
#include "helper.h"
#include <cstdio>
#include <cassert>
#include <errno.h>
#include <process.h>
#include <Psapi.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <Windowsx.h>

namespace {

enum DesktopID {
        LEFT_DESKTOP,
        RIGHT_DESKTOP,
        TOP_DESKTOP,
        BOTTOM_DESKTOP,
        MAX_DESKTOPS_ALLOWED
};

const TCHAR* g_desktop_names[] = {
        _T("default"),
        _T("lausai_desktop_right"),
        _T("lausai_desktop_top"),
        _T("lausai_desktop_bottom"),
};

const UINT ICON_UID = 100;

HWND g_dialog_list[MAX_DESKTOPS_ALLOWED]   = {};
HDESK g_desktop_list[MAX_DESKTOPS_ALLOWED] = {};
HANDLE g_thread_list[MAX_DESKTOPS_ALLOWED] = {};

// Application defined messages
const UINT ICON_MESSAGE    = WM_APP + 1;


//-------------------------------------------------
//        Helper functions for global lists
//-------------------------------------------------
void SaveDialogAndDesktop(HWND dialog, HDESK desktop, DesktopID desktop_id)
{
        g_dialog_list[desktop_id]  = dialog;
        g_desktop_list[desktop_id] = desktop;
}

int GetDesktopIdBy(HWND dialog)
{
        for (int i = 0; i < MAX_DESKTOPS_ALLOWED; i++) {
                if (g_dialog_list[i] == dialog)
                        return i;
        }
        
        printf("GetDesktopIdBy fail! no dialog=%x in the list\n", dialog);
        return -1;
}

void WaitForOtherThreads()
{
        for (int i = 1; i < MAX_DESKTOPS_ALLOWED; i++) {
                if (!g_thread_list[i]) continue;
                
                // Call WaitForSingleObject is safe even if the thread
                // already exit, if that the function will return
                // immediately.
                if (WAIT_FAILED == WaitForSingleObject(g_thread_list[i],
                                                       1000))
                        PrintErrorWith(_T("WaitForSingleObject fail!"));
        }
}

void CloseThreadHandles()
{
        for (int i = 1; i < MAX_DESKTOPS_ALLOWED; i++) {
                if (g_thread_list[i])
                        CloseHandle(g_thread_list[i]);
        }
}

void PostQuitToOtherThreads()
{
        for (int i = 0; i < MAX_DESKTOPS_ALLOWED; i++) {
                if (g_dialog_list[i]) {
                        BOOL result = PostMessage(g_dialog_list[i], WM_QUIT, 0, 0);

                        if (!result)
                                PrintErrorWith(_T("PostMessage fail!"));
                }
        }
}


//-------------------------------------------------
//             Other helper functions 
//-------------------------------------------------
bool GetStartUpPath(TCHAR* buf, size_t buf_len)
{
        if (!SHGetSpecialFolderPath(NULL, buf, CSIDL_STARTUP, FALSE)) {
                PrintErrorWith(_T("SHGetSpecialFolderPath fail!"));
                return false;
        }

        _tcsncat(buf,
                 _T("\\MultiDesktops.lnk"), 
                 buf_len - lstrlen(buf) - 1);

        return true;
}

bool IsAutoStartEnabled()
{
        TCHAR startup_path[MAX_PATH] = {};

        if (!GetStartUpPath(startup_path, _countof(startup_path)))
                return false;

        return PathFileExists(startup_path) ? true : false;
}

bool EnableAutoStart()
{
        TCHAR exe_path[MAX_PATH] = {};

        if (!GetModuleFileName(NULL, exe_path, _countof(exe_path))) {
                PrintErrorWith(_T("GetModuleFileName fail!"));
                return false;
        }

        HRESULT result = CreateSystemShortcut(_T("MultiDesktops"),
                                              CSIDL_STARTUP,
                                              exe_path);
        
        return FAILED(result) ? false : true;
}

bool DisableAutoStart()
{
        TCHAR startup_path[MAX_PATH] = {};

        if (!GetStartUpPath(startup_path, _countof(startup_path)))
                return false;
        
        if (-1 == _tremove(startup_path))
                return (ENOENT == errno) ? true : false;
        else
                return true;
}

void ShowContextMenu(HWND window, HINSTANCE instance)
{
        POINT pt = {};

        GetCursorPos(&pt);
        
        HMENU menu       = LoadMenu(instance, MAKEINTRESOURCE(IDR_MENU1));
        HMENU popup_menu = GetSubMenu(menu, 0);
        UINT check_state = IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED;
        
        if (-1 == CheckMenuItem(popup_menu, IDM_AUTOSTART, check_state))
                PrintErrorWith(_T("CheckMenuItem fail!"));

        // Must call SetForegroundWindow else the menu will not disappear 
        // when the mouse click outside the menu
        SetForegroundWindow(window);

        // The window procedure of the window argument will receive all
        // messages comes from the shoutcut menu
        TrackPopupMenu(popup_menu, TPM_LEFTALIGN, pt.x, pt.y, 0, window, NULL);

        DestroyMenu(popup_menu);
        DestroyMenu(menu);
}

// Create explorer in specific desktop
bool CreateExplorerIn(const TCHAR* desktop_name, PROCESS_INFORMATION* process_info)
{
        STARTUPINFO start_info = {sizeof(start_info)};
        WCHAR buf[64]          = {};

        _tcsncat(buf, desktop_name, _countof(buf) - 1);
        start_info.lpDesktop = buf;

        if (!CreateProcess(_T("C:\\Windows\\explorer.exe"),
                           NULL,
                           NULL,
                           NULL,
                           FALSE,
                           0,
                           NULL,
                           NULL,
                           &start_info,
                           process_info)) 
        {
                PopupErrorWith(_T("Create explorer fail!"));
                return false;
        }
        return true;
}

HDESK CreateDesktopBy(DesktopID desktop_id)
{
        DWORD desktop_flags = DESKTOP_CREATEWINDOW | 
                              DESKTOP_CREATEMENU | 
                              DESKTOP_SWITCHDESKTOP;

        const TCHAR* desktop_name = NULL;

        switch (desktop_id) {
        case LEFT_DESKTOP:
                desktop_name = g_desktop_names[LEFT_DESKTOP];
                break;
        case RIGHT_DESKTOP:
                desktop_name = g_desktop_names[RIGHT_DESKTOP];
                break;
        case TOP_DESKTOP:
                desktop_name = g_desktop_names[TOP_DESKTOP];
                break;
        case BOTTOM_DESKTOP:
                desktop_name = g_desktop_names[BOTTOM_DESKTOP];
                break;
        default:
                ;
        }

        HDESK desktop = CreateDesktop(desktop_name,
                                      NULL, 
                                      NULL, 
                                      0, 
                                      desktop_flags,
                                      NULL);
        if (desktop) {
                return desktop;
        } else {
                PrintErrorWith(_T("CreateDesktop fail!"));
                return NULL;
        }
}

void WaitForInputIdle2(HANDLE process, DWORD timeout)
{
        DWORD result = WaitForInputIdle(process, timeout);

        switch (result) {
        case 0:
                puts("WaitForInputIdle success");
                return;
        case WAIT_TIMEOUT:
                puts("WaitForInputIdle timeout!");
                return;
        case WAIT_FAILED:
                PrintErrorWith(_T("WaitForInputIdle fail!"));
                return;
        default:
                printf("WaitForInputIdle return=%u\n", result);
        }
}

WORD GetIconIdBy(int desktop_id)
{
        switch (desktop_id) {
        case LEFT_DESKTOP:
                return IDI_ICON4;
        case RIGHT_DESKTOP:
                return IDI_ICON5;
        case TOP_DESKTOP:
                return IDI_ICON6;
        case BOTTOM_DESKTOP:
                return IDI_ICON3;
        default:
                assert(false && "wrong desktop id");
                ;
        }
        return 0;
}

const TCHAR* GetTipBy(int desktop_id)
{
        switch (desktop_id) {
        case LEFT_DESKTOP:
                return _T("Desktop1");
        case RIGHT_DESKTOP:
                return _T("Desktop2");
        case TOP_DESKTOP:
                return _T("Desktop3");
        case BOTTOM_DESKTOP:
                return _T("Desktop4");
        default:
                assert(false && "wrong desktop id");
                ;
        }
        return 0;
}

HWND SwitchToDesktop(DesktopID desktop_id)
{
        HWND dialog   = NULL;
        HDESK desktop = CreateDesktopBy(desktop_id);
        PROCESS_INFORMATION process_info = {};

        if (!desktop) return NULL;
        
        // The desktop cannot be set of the threads which contained any
        // windows or hooks, we must set desktop before create our dialog
        if (!SetThreadDesktop(desktop)) {
                PrintErrorWith(_T("SetThreadDesktop fail!"));
                goto error_out;
        }

        if (!SwitchDesktop(desktop)) {
                PopupErrorWith(_T("Switch desktop fail!"));
                goto error_out;
        }
        
        // We create our dialog before creating the explorer process on purpose.
        // This is a little trick here, the dialog register to system that it 
        // receives the taskbar restart message.
        // If the explorer doesn't exist, we created it and the dialog procedure
        // will receive a message and add tray icon to taskbar.
        // If the explorer exists, we add tray icon directly.
        dialog = GetTrayDialog();
        if (!dialog)
                goto error_out;
        
        if (!GetShellWindow()) {
                if (!CreateExplorerIn(g_desktop_names[desktop_id], &process_info))
                        goto error_out;

                WaitForInputIdle2(process_info.hProcess, 2000);
                CloseHandle(process_info.hThread);
                CloseHandle(process_info.hProcess);
        } else {
                AddTrayIcon(dialog,
                            GetIconIdBy(desktop_id),
                            ICON_UID, 
                            ICON_MESSAGE,
                            GetTipBy(desktop_id),
                            5000);
        }

        SaveDialogAndDesktop(dialog, desktop, desktop_id);
        return dialog;

error_out:
        if (desktop) CloseDesktop(desktop);
        if (dialog) DestroyWindow(dialog);
        if (process_info.hProcess) CloseHandle(process_info.hProcess);
        if (process_info.hThread) CloseHandle(process_info.hThread);
        return NULL;
}

void RegisterHotKeys(HWND dialog)
{
        if (!RegisterHotKey(dialog, LEFT_DESKTOP, MOD_CONTROL, VK_LEFT))
                PrintErrorWith(_T("RegisterHotKey fail!"));

        if (!RegisterHotKey(dialog, RIGHT_DESKTOP, MOD_CONTROL, VK_RIGHT))
                PrintErrorWith(_T("RegisterHotKey fail!"));
 
        if (!RegisterHotKey(dialog, TOP_DESKTOP, MOD_CONTROL, VK_UP))
                PrintErrorWith(_T("RegisterHotKey fail!"));
 
        if (!RegisterHotKey(dialog, BOTTOM_DESKTOP, MOD_CONTROL, VK_DOWN))
                PrintErrorWith(_T("RegisterHotKey fail!"));
}

void UnregisterHotKeys(HWND dialog)
{
        UnregisterHotKey(dialog, LEFT_DESKTOP);
        UnregisterHotKey(dialog, RIGHT_DESKTOP);
        UnregisterHotKey(dialog, TOP_DESKTOP);
        UnregisterHotKey(dialog, BOTTOM_DESKTOP);
}


//-------------------------------------------------
//    Dialog procedure related core functions 
//-------------------------------------------------
BOOL HandleWmCommand(WPARAM wParam)
{
        switch (wParam) {
        case IDM_AUTOSTART:
                if (IsAutoStartEnabled())
                        DisableAutoStart();
                else
                        EnableAutoStart();

                break;
        case IDM_EXIT:
                puts("IDM_EXIT");
                PostQuitToOtherThreads();
                return TRUE;
        case IDM_ABOUT:
                MessageBox(NULL,
                           _T("Please use\n")
                           _T("Ctrl + Up\n")
                           _T("Ctrl + Down\n")
                           _T("Ctrl + Right\n")
                           _T("Ctrl + Left\n")
                           _T("to switch to other desktops"),
                           _T("About"), 
                           MB_OK);
                break;
        default:
                ;
        }

        return FALSE;
}

// Every thread is associated with one desktop, one dialog and one icon
unsigned WINAPI StartTrayIcon(void* arg)
{
        HWND dialog = NULL;

        if (NULL == arg) {
                dialog = SwitchToDesktop(LEFT_DESKTOP);
        } else {
                DesktopID desktop_id = *(reinterpret_cast<DesktopID*>(&arg));
                dialog               = SwitchToDesktop(desktop_id);
        }

        if (!dialog) return 1;

        BOOL ret;
        MSG  msg = {};

        while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
                if (ret == -1) {
                        PrintError();
                } else if (!IsDialogMessage(dialog, &msg)) {
                        TranslateMessage(&msg); 
                        DispatchMessage(&msg); 
                }
        }

        if (NULL == arg) {
                UnregisterHotKeys(dialog);
                WaitForOtherThreads();
                CloseThreadHandles();
        }

        DeleteTrayIcon(dialog, 5000);
        DestroyWindow(dialog);

        puts("thread exit");
        return 0;
}

void HandleHotKey(int desktop_id)
{
        static int current_desktop = LEFT_DESKTOP;
        
        if (current_desktop == desktop_id) {
                puts("already in the corrent desktop");
                return;
        }

        if (g_desktop_list[desktop_id]) {
                // The desktop already exist, just switch to it
                if (!SwitchDesktop(g_desktop_list[desktop_id])) {
                        PrintErrorWith(_T("SwitchDesktop fail!"));
                        return;
                }
        } else {
                unsigned tid     = 0;
                uintptr_t result = _beginthreadex(NULL,
                                                  0,
                                                  StartTrayIcon,
                                                  reinterpret_cast<void*>(desktop_id),
                                                  0,
                                                  &tid);

                HANDLE thread = reinterpret_cast<HANDLE>(result);
                                                         
                if (thread)
                        g_thread_list[desktop_id] = thread;
                else
                        puts("create thread fail");
        }
        current_desktop = desktop_id;
}

void SwitchDesktopByButton(DesktopID desktop_id, HWND dialog)
{
        HandleHotKey(desktop_id);
        DestroyWindow(dialog);
}

BOOL CALLBACK DialogHandler(HWND dialog, UINT uMsg, WPARAM wParam, LPARAM)
{
        switch (uMsg) {
        case WM_COMMAND:
                switch (wParam) {
                case IDC_BUTTON1:
                        SwitchDesktopByButton(LEFT_DESKTOP, dialog);
                        break;
                case IDC_BUTTON2:
                        SwitchDesktopByButton(RIGHT_DESKTOP, dialog);
                        break;
                case IDC_BUTTON3:
                        SwitchDesktopByButton(TOP_DESKTOP, dialog);
                        break;
                case IDC_BUTTON4:
                        SwitchDesktopByButton(BOTTOM_DESKTOP, dialog);
                        break;
                default:
                        ;
                }
                break;
        case WM_ACTIVATE:
                if (WA_INACTIVE == LOWORD(wParam))
                        DestroyWindow(dialog);

                break;
        default:
                ;
        }

        return FALSE;
}

BOOL HandleIconMessage(HWND dialog, LPARAM lParam)
{
        HWND dialog2 = NULL;
 
        switch (lParam) {
        case WM_RBUTTONUP:
                ShowContextMenu(dialog, GetModuleHandle(NULL));
                break;
        case WM_LBUTTONUP:
                dialog2 = CreateDialog(GetModuleHandle(NULL),
                                       MAKEINTRESOURCE(IDD_DIALOG1),
                                       NULL,
                                       DialogHandler);
                if (dialog2) {
                        RECT rect   = {};
                        POINT point = {};

                        ShowWindow(dialog2, SW_SHOW);
                        if (!GetWindowRect(dialog2, &rect))
                                PrintErrorWith(_T("GetWindowRect fail!"));
                        
                        GetPointAlignTaskbar(&point,
                                             rect.right - rect.left,
                                             rect.bottom - rect.top);
                        if (!MoveWindow(dialog2,
                                        point.x,
                                        point.y,
                                        rect.right - rect.left,
                                        rect.bottom - rect.top,
                                        TRUE))
                                PrintErrorWith(_T("MoveWindow fail!"));
                } else {
                        PopupErrorWith(_T("CreateDialog fail!"));
                }
                break;
        default:
                ;
        }

        return FALSE;
}

BOOL CALLBACK MenuHandler(HWND dialog, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        //printf("receive dialog message uMsg=0x%x wParam=%u lParam=%u\n", uMsg, wParam, lParam);
        static UINT taskbar_restart   = 0;
        static bool hotkey_registered = false;

        switch (uMsg) {
        case WM_INITDIALOG:
                taskbar_restart = RegisterWindowMessage(_T("TaskbarCreated"));

                if (!hotkey_registered) {
                        RegisterHotKeys(dialog);
                        hotkey_registered = true;
                }
                break;
        case WM_COMMAND:
                return HandleWmCommand(wParam);
        case WM_HOTKEY:
                HandleHotKey(wParam);
                break;
        case ICON_MESSAGE:
                return HandleIconMessage(dialog, lParam);
        default:
                if (uMsg == taskbar_restart) {
                        printf("taskbar restarted! add tray icon again!\n");
                        int desktop_id = GetDesktopIdBy(dialog);
                        WORD icon_id   = GetIconIdBy(desktop_id);

                        AddTrayIcon(dialog,
                                    icon_id,
                                    ICON_UID,
                                    ICON_MESSAGE,
                                    GetTipBy(desktop_id),
                                    5000);
                }
        }

        return FALSE;
}

}       // End of namespace


HWND GetTrayDialog()
{
        HINSTANCE instance = GetModuleHandle(NULL);

        HWND dialog = CreateDialog(instance,
                                   MAKEINTRESOURCE(IDD_DIALOG3),
                                   NULL,
                                   MenuHandler);

        if (!dialog) {
                PrintErrorWith(_T("create simple tray icon dialog fail!"));
                return NULL;
        }
                
        return dialog;
}

void StartMultiDesltops()
{
        if (AnotherInstanceRunning()) {
                MessageBox(NULL, _T("Program already running"), _T("Alert"), MB_ICONWARNING);
                return;
        }

        StartTrayIcon(NULL);
        puts("program exist!");
}
