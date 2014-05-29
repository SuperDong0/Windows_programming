#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <windows.h>

// Because the explorer.exe sometimes is not reliable,
// so we use retry method and user can set the timeout
// in millisecond.

bool AddTrayIcon(HWND window, 
                 WORD icon_id, 
                 UINT icon_uid, 
                 UINT icon_message,
                 const TCHAR* tip,
                 DWORD timeout);

bool DeleteTrayIcon(HWND window, DWORD timeout);
bool ModifyTrayIcon(HWND window, WORD icon_id, DWORD timeout);

#endif  // End of header guard
