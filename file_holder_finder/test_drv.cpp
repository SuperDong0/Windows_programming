#define _CRT_SECURE_NO_WARNINGS

#include "resource.h"
#include <cstdio>
#include <clocale>
#include <cstring>
#include <Windows.h>
#include <tchar.h>


void Error(const char* str_error)
{
        printf("\n%s\n", str_error);
        printf("error code=%d\n", GetLastError());
}

enum DriverAction {
        Install,
        Read,
        Write,
        IoControl,
        Uninstall 
};


bool UninstallDriver(const TCHAR* drv_name)
{
        bool ret              = false;
        SC_HANDLE drv         = NULL;
        SC_HANDLE sc_manager  = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        SERVICE_STATUS status = {};

        if (!sc_manager) {
                Error("Create SC manager fail!");
                return false;
        }

        drv = OpenService(sc_manager, drv_name, SERVICE_ALL_ACCESS);
        
        if (!drv) {
                Error("Open driver service fail!");
                goto error_out;
        }
        
        // This will invoke the DriverUnload routine 
        if (!ControlService(drv, SERVICE_CONTROL_STOP, &status))
                Error("Stop driver fail!");
        
        if (!DeleteService(drv))
                Error("Delete driver fail!");
        
        ret = true;
error_out:
        if (drv)
                CloseServiceHandle(drv);
        if (sc_manager)
                CloseServiceHandle(sc_manager);

        return ret;
}

void ReadDriver()
{
        HANDLE device = CreateFile("\\\\.\\ObjNameGetter",
                                   GENERIC_READ,
                                   0,      // Not allow share
                                   NULL,   // No security
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);

        if (INVALID_HANDLE_VALUE == device) {
                Error("Create device fail!");
                return;
        }
        
        TCHAR buf[8]     = {};
        DWORD bytes_read = 0;

        BOOL result = ReadFile(device, buf, _countof(buf), &bytes_read, NULL);

        printf("ReadFile return %d, read %u bytes\n", result, bytes_read);
        CloseHandle(device);
}

void WriteDriver()
{
        HANDLE device = CreateFile("\\\\.\\ObjNameGetter",
                                   GENERIC_WRITE,
                                   0,      // Not allow share
                                   NULL,   // No security
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);

        if (INVALID_HANDLE_VALUE == device) {
                Error("Create device fail!");
                return;
        }
        
        TCHAR buf[8]      = {};
        DWORD bytes_write = 0;

        BOOL result = WriteFile(device, buf, _countof(buf), &bytes_write, NULL);

        printf("WriteFile return %d, write %u bytes\n", result, bytes_write);
        CloseHandle(device);
}

void IoControlDriver()
{
        HANDLE device = CreateFile("\\\\.\\ObjNameGetter",
                                   GENERIC_READ | GENERIC_WRITE,
                                   0,      // Not allow share
                                   NULL,   // No security
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);

        if (INVALID_HANDLE_VALUE == device) {
                Error("Create device fail!");
                return;
        }
        
        TCHAR buf_read[16] = {};
        TCHAR buf_write[8] = {};
        DWORD bytes_read   = 0;
        DWORD ctl_code     = CTL_CODE(FILE_DEVICE_UNKNOWN,
                                      2048,
                                      METHOD_BUFFERED,
                                      FILE_ANY_ACCESS);

        BOOL result = DeviceIoControl(device,
                                      ctl_code,
                                      buf_read,
                                      _countof(buf_read),
                                      buf_write,
                                      _countof(buf_write),
                                      &bytes_read,
                                      NULL);
        
        printf("IoControl return %d, read %u bytes\n", result, bytes_read);
        CloseHandle(device);
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

void* ExtractDriver(DWORD* size)
{
        HRSRC res = FindResource(NULL, MAKEINTRESOURCE(IDR_BINRES1), "BINRES");
        if (!res)
                Error("Find resource fail!");
        
        HGLOBAL drv_res = LoadResource(NULL, res);
        if (!res) {
                Error("Load resource fail!");
                return NULL;
        }

        *size = SizeofResource(NULL, res);
        if (0 == *size) {
                Error("Get resource size fail!");
                return NULL;
        }
        
        void* drv_data = LockResource(drv_res);
        if (!drv_data)
                Error("Load resource fail!");

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
                Error("Create SC manager fail!");
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
                        Error("Create driver service fail!");
                        goto error_out;
                }

                // The driver service already exist, we can just open it
                drv = OpenService(sc_manager, drv_name, SERVICE_ALL_ACCESS);

                if (!drv) {
                        Error("Open driver service fail!");
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
        remove(drv_path);

        if (drv)
                CloseServiceHandle(drv);

        if (sc_manager)
                CloseServiceHandle(sc_manager);

        return ret;
}


int main()
{
        int action = 0;
        const char* prompt = "\nPlease enter action:\n"
                             "0: Install driver\n"
                             "1: Read data\n"
                             "2: Write data\n"
                             "3: IoControl driver\n"
                             "4: Uninstall driver";
        puts(prompt);
        
        const TCHAR* drv_name = _T("ObjNameGetter");

        while (EOF != scanf("%d", &action)) {
                switch (action) {
                case Install:
                        InstallDriver(drv_name);
                        break;
                case Read:
                        ReadDriver();
                        break;
                case Write:
                        WriteDriver();
                        break;
                case Uninstall:
                        UninstallDriver(drv_name);
                        break;
                case IoControl:
                        IoControlDriver();
                        break;
                default:
                        ;
                }

                puts(prompt);
        }
        return 0;
}
