#define _CRT_SECURE_NO_WARNINGS

#include "bho.h"
#include "helper.h"
#include <cstdio>
#include <clocale>
#include <map>
#include <string>
#include <Windows.h>
#include <tchar.h>

using std::map;
using std::wstring;

namespace {

void TestMutex()
{
        HANDLE mutex = CreateMutex(NULL, TRUE, _T("ie_http_hook"));
        if (!mutex) {
                puts("get mutex fail!");
                return;
        }

        if (ERROR_ALREADY_EXISTS == GetLastError())
                WaitForSingleObject(mutex, INFINITE);

        Sleep(1000);
        puts("kerker");
        Sleep(1000);
        puts("kerker");
        Sleep(1000);
        puts("kerker");
        // Do the actions here

        ReleaseMutex(mutex);
        CloseHandle(mutex);
}

struct PipeData {
        DWORD pid_;
        DWORD count_;
};

int TestPipe()
{
        HANDLE mutex = CreateMutex(NULL, TRUE, _T("ie_http_hook"));
        if (!mutex) {
                puts("get mutex fail!");
                return 0;
        }
        
        TCHAR* pipe_name = _T("\\\\.\\pipe\\my_named_pipe");

        if (ERROR_ALREADY_EXISTS == GetLastError()) {
                // Pipe client
                DWORD pid = GetCurrentProcessId();

                for (DWORD i = 0; i < 0x999999999; i++) {
                        HANDLE pipe = CreateFile(pipe_name,
                                                 GENERIC_WRITE,
                                                 0,
                                                 NULL,
                                                 OPEN_EXISTING,
                                                 0,
                                                 NULL);
                        if (INVALID_HANDLE_VALUE == pipe) {
                                puts("open named pipe fail!");
                                return 0;
                        }
                        
                        PipeData pipe_data = {pid, i};
                        DWORD bytes_write  = 0;
                        
                        BOOL result = WriteFile(pipe, &pipe_data, sizeof(pipe_data), &bytes_write, NULL);
                        if (!result) 
                                PrintErrorWith(_T("WriteFile fail!"));
                        
                        CloseHandle(pipe);
                        Sleep(1000);
                }

        } else {
                // Pipe server
                DWORD buf_size = 4096;
                HANDLE pipe = CreateNamedPipe(pipe_name,
                                              PIPE_ACCESS_INBOUND,
                                              PIPE_TYPE_BYTE | PIPE_WAIT,
                                              PIPE_UNLIMITED_INSTANCES,
                                              buf_size,
                                              buf_size,
                                              0,
                                              NULL);
                if (INVALID_HANDLE_VALUE == pipe) {
                        puts("create pipe fail!");
                        return 0;
                }
                
                for (int i = 0; i < 0x99999999; i++) {
                        ConnectNamedPipe(pipe, NULL);
                        
                        PipeData pipe_data = {};
                        DWORD bytes_read   = 0;

                        BOOL result = ReadFile(pipe, &pipe_data, sizeof(pipe_data), &bytes_read, NULL);
                        if (result)
                                printf("pid=%u count=%u\n", pipe_data.pid_, pipe_data.count_);
                        else
                                PrintErrorWith(_T("ReadFile fail!"));
                        
                        DisconnectNamedPipe(pipe);
                }

                CloseHandle(pipe);
        }

        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 0;
}

void DoReadFile(HANDLE handle, void* buffer, size_t buffer_size)
{
        DWORD bytes_read = 0;

        BOOL result = ReadFile(handle, buffer, buffer_size, &bytes_read, NULL);

        if (!result) {
                PrintErrorWith(_T("ReadFile fail!"));
                return;
        }

        if (bytes_read != buffer_size)
                printf("ReadFile only read %u bytes\n", bytes_read);
}

}       // End of unnamed namespace

int main()
{
        setlocale(LC_ALL, "");

//        puts("Register BHO dll, please enter Y.\n"
//             "Unregister BHO dll, please enter N.");
//        
//        int ch = getchar();
//
//        if ('Y' == ch || 'y' == ch)
//                system("regsvr32 bho.dll");
//        else if ('N' == ch || 'n' == ch)
//                system("regsvr32 /u bho.dll");

        puts("Wait for http request record...");
        
        HANDLE pipe = CreateNamedPipe(_T("\\\\.\\pipe\\hook_http_pipe"),
                                      PIPE_ACCESS_INBOUND,
                                      PIPE_TYPE_BYTE | PIPE_WAIT,
                                      PIPE_UNLIMITED_INSTANCES,
                                      0,
                                      0,
                                      0,
                                      NULL);

        if (INVALID_HANDLE_VALUE == pipe) {
                puts("create pipe fail!");
                return 0;
        }
        
        map<HINTERNET, wstring> url_list;
        map<HINTERNET, wstring>::iterator it;

        for (size_t i = 0; i < 0x99999999; i++) {
                ConnectNamedPipe(pipe, NULL);
                
                HttpInfo http_info = {};
                TCHAR buf[2048]    = {};
                int offset         = 0;

                DoReadFile(pipe, &http_info, sizeof(http_info));
                switch (http_info.status_) {
                case Connect:
                        if (80 == http_info.port_) {
                                _tcsncat(buf, _T("http://"), _countof(buf));
                                offset = 7;
                        } else if (443 == http_info.port_) {
                                _tcsncat(buf, _T("https://"), _countof(buf));
                                offset = 8;
                        }
                                
                        if (http_info.data_len_)
                                DoReadFile(pipe, &buf[offset], http_info.data_len_ * sizeof(TCHAR));

                        url_list[http_info.http_handle_] = buf;
                        break;
                case OpenRequest:
                        if (http_info.data_len_)
                                DoReadFile(pipe, buf, http_info.data_len_ * sizeof(TCHAR));
                        
                        it = url_list.find(http_info.http_handle_);
                        if (url_list.end() != it)
                                printf("\n%ls%ls\n", it->second.c_str(), buf);
                        else
                                puts("Can't find url for OpenRequest!!!!!!!!!!!!!!!!!!!");

                        break;
                case Close:
                        url_list.erase(http_info.http_handle_);
                        break;
                default:
                        ;
                }
               
                DisconnectNamedPipe(pipe);
        }

        CloseHandle(pipe);

        return 0;
}
