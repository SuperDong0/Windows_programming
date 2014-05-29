#define _CRT_SECURE_NO_WARNINGS

#include "resource.h"
#include "helper.h"
#include "enum_open_files.h"
#include <cstdio>
#include <clocale>
#include <Psapi.h>
#include <Commctrl.h>
#include <Shlwapi.h>

using std::vector;
using std::wstring;

namespace {

bool (*FindFileHolderProc)(vector<wstring>* file_list,
                           vector<ProcessInfo>* file_holder) = FindFileHolder;

//-------------------------------------------------
//                  Driver related
//-------------------------------------------------
void* ExtractDriver(DWORD* size)
{
        HRSRC res = FindResource(NULL, MAKEINTRESOURCE(IDR_BINRES1), _T("BINRES"));
        if (!res)
                PrintErrorWith("Find resource fail!");
        
        HGLOBAL drv_res = LoadResource(NULL, res);
        if (!res) {
                PrintErrorWith("Load resource fail!");
                return NULL;
        }

        *size = SizeofResource(NULL, res);
        if (0 == *size) {
                PrintErrorWith("Get resource size fail!");
                return NULL;
        }
        
        void* drv_data = LockResource(drv_res);
        if (!drv_data)
                PrintErrorWith("Load resource fail!");

        return drv_data;
}

bool InstallDriver(const TCHAR* drv_name)
{
        // Extract driver from resource and write it to disk
        DWORD drv_size           = 0;
        TCHAR drv_path[MAX_PATH] = {};
        void* drv_data           = ExtractDriver(&drv_size);

        if (!drv_data)
                return false;

        GetTempFilePath(drv_path, _countof(drv_path), _T("drv"));

        FILE* f = _tfopen(drv_path, _T("wb"));

        fwrite(drv_data, drv_size, 1, f);
        fclose(f);
        
        // Open SC manager and load the driver to kernel
        bool ret             = false;
        SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        SC_HANDLE drv        = NULL;

        if (!sc_manager) {
                PrintErrorWith("Create SC manager fail!");
                goto error_out;
        }
        
        drv = CreateService(sc_manager,
                            drv_name,
                            drv_name,
                            SERVICE_ALL_ACCESS,
                            SERVICE_KERNEL_DRIVER,
                            SERVICE_DEMAND_START,
                            SERVICE_ERROR_IGNORE,
                            drv_path,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL);
        if (!drv) {
                DWORD err = GetLastError();

                if (ERROR_IO_PENDING != err && ERROR_SERVICE_EXISTS != err) {
                        PrintErrorWith("Create driver service fail!");
                        goto error_out;
                }

                // The driver service already exist, we can just open it
                drv = OpenService(sc_manager, drv_name, SERVICE_ALL_ACCESS);

                if (!drv) {
                        PrintErrorWith("Open driver service fail!");
                        goto error_out;
                }
        }
        
        // This will invoke the DriverEntry routine
        if (!StartService(drv, NULL, NULL)) {
                DWORD err = GetLastError();

                if (ERROR_IO_PENDING == err) {
                        puts("start driver serivce fail, ERROR_IO_PENDING");
                        goto error_out;
                }

                if (ERROR_SERVICE_ALREADY_RUNNING == err)
                        puts("service already running!");
        }
        
        ret = true;
error_out:
        _tremove(drv_path);

        if (drv)
                CloseServiceHandle(drv);

        if (sc_manager)
                CloseServiceHandle(sc_manager);

        return ret;
}

bool UninstallDriver(const TCHAR* drv_name)
{
        bool ret              = false;
        SC_HANDLE drv         = NULL;
        SC_HANDLE sc_manager  = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        SERVICE_STATUS status = {};

        if (!sc_manager) {
                PrintErrorWith("Create SC manager fail!");
                return false;
        }

        drv = OpenService(sc_manager, drv_name, SERVICE_ALL_ACCESS);
        
        if (!drv) {
                PrintErrorWith("Open driver service fail!");
                goto error_out;
        }
        
        // This will invoke the DriverUnload routine 
        if (!ControlService(drv, SERVICE_CONTROL_STOP, &status))
                PrintErrorWith("Stop driver fail!");
        
        if (!DeleteService(drv))
                PrintErrorWith("Delete driver fail!");
        
        ret = true;
error_out:
        if (drv)
                CloseServiceHandle(drv);
        if (sc_manager)
                CloseServiceHandle(sc_manager);

        return ret;
}


//-------------------------------------------------
//                  GUI related
//-------------------------------------------------
bool GetProcessImageDevicePath(DWORD pid, WCHAR* buf, size_t buf_len)
{
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION,
                                     FALSE,
                                     pid);
        if (!process) {
                PrintErrorWith("OpenProcess fail!");
                return false;
        }
        
        bool ret = false;

        if (GetProcessImageFileNameW(process, buf, buf_len))
                ret = true;
        else
                PrintErrorWith("GetProcessImageFileNameW fail!");
        
        CloseHandle(process);
        return ret;
}

void InsertListItem(HWND list_view, const ProcessInfo& process_info)
{
        LVITEM lv_item       = {};
        WCHAR buf[MAX_PATH]  = {};
        
        lv_item.mask       = LVIF_TEXT;
        lv_item.pszText    = _T("");
        lv_item.cchTextMax = lstrlen(lv_item.pszText);
        lv_item.iItem      = 0;
        ListView_InsertItem(list_view, &lv_item);

        ListView_SetItemText(list_view,
                             0,
                             2,
                             const_cast<WCHAR*>(process_info.file_path_.c_str()));
        
        if (GetProcessImageDevicePath(process_info.pid_, buf, _countof(buf))) {
                PathStripPath(buf);
                ListView_SetItemText(list_view, 0, 0, buf);
        } else {
                ListView_SetItemText(list_view, 0, 0, _T("Can't get name"));
        }

        _snwprintf(buf, _countof(buf) - 1, _T("%u"), process_info.pid_);
        ListView_SetItemText(list_view, 0, 1, buf);
}

bool DotDirectory(const WCHAR* dir)
{
        if (1 == wcslen(dir) && dir[0] == L'.')
                return true;

        if (2 == wcslen(dir) && dir[0] == L'.' && dir[1] == L'.')
                return true;
        
        return false;
}

void FindFileHolderInDirectory(const WCHAR* dir,
                               vector<ProcessInfo>* file_holder_list)
{
        WIN32_FIND_DATAW data = {};
        const size_t buf_len  = MAX_PATH + (2 * sizeof(WCHAR));
        WCHAR buf[buf_len]    = {};
        
        swprintf(buf, _countof(buf), L"%ls\\*", dir);

        HANDLE find = FindFirstFileW(buf, &data);
        
        if (INVALID_HANDLE_VALUE == find) {
                PrintErrorWith("FindFirstFileW fail!");
                return;
        }
        
        PathRemoveFileSpec(buf);
        vector<wstring> file_list;

        do {
                WCHAR file_name[MAX_PATH] = {};

                PathCombineW(file_name, buf, data.cFileName);
 
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        if (DotDirectory(data.cFileName))
                                continue;
                        
                        if (PathIsDirectoryEmptyW(file_name))
                                continue;

                        FindFileHolderInDirectory(file_name, file_holder_list);
                } else {
                        file_list.push_back(file_name);
                }
        } while (FindNextFileW(find, &data));

        FindClose(find);
        FindFileHolderProc(&file_list, file_holder_list);
}

BOOL HandleWmCommand(HWND dialog, WPARAM wParam)
{
        static bool driver_installed = false;
        const TCHAR* drv_name        = _T("ObjNameGetter");

        // Use low-word part, because the high-word part of keyboard accelerator
        // and menu are 1 and 0 respectively.
        switch (LOWORD(wParam)) {
        case IDC_SELECTFILE:
        case IDM_OPEN1: {
                TCHAR file[MAX_PATH] = {};

                bool result = GetOpenFileNameByFilter(dialog,
                                                      _T("All\0*.*\0"),
                                                      NULL,
                                                      NULL,
                                                      file,
                                                      _countof(file));
                if (result)
                        SetDlgItemText(dialog, IDC_EDIT1, file);

                break;
        }
        case IDCANCEL:
        case IDM_CLOSE2:
                if (driver_installed)
                        UninstallDriver(drv_name);

                if (!DestroyWindow(dialog))
                        PopupError();

                PostQuitMessage(0);
                return TRUE;
        case IDC_SEARCH: {
                HWND button    = GetDlgItem(dialog, IDC_SEARCH);
                HWND list_view = GetDlgItem(dialog, IDC_LIST1);

                EnableWindow(button, FALSE);
                ListView_DeleteAllItems(list_view);

                WCHAR text[MAX_PATH] = {};
                GetDlgItemTextW(dialog, IDC_EDIT1, text, _countof(text));
                
                // Since user can type the path manually, so we need to
                // check if the file exists first.
                if (!PathFileExistsW(text)) {
                        EnableWindow(button, TRUE);
                        break;
                }
                
                // The path exists in the file system, now we can intstall the
                // driver.
                static bool tried_install_driver = false;
                vector<ProcessInfo> file_holder_list;

                if (!tried_install_driver && InstallDriver(drv_name)) {
                        puts("Install driver success!");
                        FindFileHolderProc = FindFileHolderByDriver;
                        driver_installed   = true;
                }
                tried_install_driver = true;
                
                WCHAR* ext = PathFindExtensionW(text);
                
                if (PathIsDirectoryW(text)) {
                        FindFileHolderInDirectory(text, &file_holder_list);
                } else if (0 == lstrcmpi(_T(".dll"), ext)) {
                        FindDllHolder(text, &file_holder_list);
                } else {
                        vector<wstring> file_list;

                        file_list.push_back(text);

                        if (driver_installed)
                                FindFileHolderByDriver(&file_list, &file_holder_list);
                        else
                                FindFileHolder(&file_list, &file_holder_list);
                }

                vector<ProcessInfo>::iterator it = file_holder_list.begin();

                for (; it != file_holder_list.end(); it++)
                        InsertListItem(list_view, *it);

                EnableWindow(button, TRUE);
                break;
        }
        case IDC_COPYPATH: {
                TCHAR buf[MAX_PATH] = {};

                GetDlgItemText(dialog, IDC_EDIT1, buf, _countof(buf));
                if (lstrlen(buf) > 0)
                        CopyToClipboard(buf);

                break;
        }
        case IDM_ABOUT:
                MessageBox(dialog,
                           _T("This program can help you find\n")
                           _T("the processes which hold the file\n")
                           _T("handle currently"), 
                           _T("About"),
                           MB_OK);
                break;
        default:
                ;
        }

        return FALSE;
}

void InitDialog(HWND dialog)
{
        HWND list_view = GetDlgItem(dialog, IDC_LIST1);

        if (!list_view) {
                PopupError();
                return;
        }
        ListView_SetExtendedListViewStyle(list_view, LVS_EX_FULLROWSELECT);

        LVCOLUMN column = {};
        column.mask     = LVCF_TEXT | LVCF_WIDTH;

        column.pszText    = _T("Process");
        column.cx         = 120;
        ListView_InsertColumn(list_view, 0, &column);

        column.pszText    = _T("Pid");
        column.cx         = 60;
        ListView_InsertColumn(list_view, 1, &column);

        column.pszText    = _T("Path");
        column.cx         = 360;
        ListView_InsertColumn(list_view, 2, &column);
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT uMsg, WPARAM wParam, LPARAM)
{
        HDROP drop     = NULL;
        UINT num_files = 0;

        switch (uMsg) {
        case WM_INITDIALOG:
                InitDialog(dialog);
                break;
        case WM_COMMAND:
                return HandleWmCommand(dialog, wParam);
        case WM_DROPFILES:
                drop      = reinterpret_cast<HDROP>(wParam);
                num_files = DragQueryFile(drop, 0xFFFFFFFF, NULL, 0);

                for (UINT i = 0; i < num_files; i++) {
                        TCHAR buf[MAX_PATH] = {};

                        if (0 != DragQueryFile(drop, i, buf, _countof(buf))) {
                                SetDlgItemText(dialog, IDC_EDIT1, buf);
                                break;
                        } else {
                                PopupError();
                        }
                }

                DragFinish(drop);
                break;
        default:
                ;
        }

        return FALSE;
}

}       // End of unnamed namespace


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
//int main()
{
        setlocale(LC_ALL, "");

        HINSTANCE instance = GetModuleHandle(NULL);
        HWND dialog        = CreateDialog(instance, MAKEINTRESOURCE(IDD_DIALOG2), NULL, DialogProc);
        HACCEL accelerator = LoadAccelerators(instance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

        if (!dialog || !accelerator) {
                PopupError();
                return EXIT_FAILURE;
        }

        BOOL ret;
        MSG  msg = {};

        while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
                if (ret == -1) {
                        PopupError();
                        break;
                }
                
                // Need to translate accelerator for hot key
                if (TranslateAccelerator(dialog, accelerator, &msg))
                        continue;
 
                if (IsDialogMessage(dialog, &msg))
                        continue;

                TranslateMessage(&msg); 
                DispatchMessage(&msg); 
        }

        return 0;
}
