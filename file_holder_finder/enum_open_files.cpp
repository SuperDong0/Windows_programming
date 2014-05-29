#define _CRT_SECURE_NO_WARNINGS

#include "enum_open_files.h"
#include "helper.h"
#include <cstdio>
#include <climits>
#include <Winternl.h>
#include <Psapi.h>
#include <Shlwapi.h>
#include <TlHelp32.h>

#define DEBUG_SYSTEM_HANDLES

// There are many handles in the system we don't have
// access right and the windows API fails for those handles.
// That will make huge logs, so we use a specific macro
// to output logs and turn on this macro when necessary.
#ifndef DEBUG_SYSTEM_HANDLES
        #define LOG(x)
#else
        #define LOG(x) PrintErrorWith(x)
#endif

using std::vector;
using std::wstring;

namespace {

//-------------------------------------------------
//         Native API related sturcure
//-------------------------------------------------
struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
        PVOID Object;
        ULONG_PTR UniqueProcessId;
        HANDLE HandleValue;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
};

struct SYSTEM_HANDLE_INFORMATION {
        ULONG_PTR num_of_handles_;
        ULONG_PTR reserved_;
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handles_[1];
};

enum OBJECT_INFORMATION_CLASS { 
        ObjectBasicInformation = 0,
        ObjectNameInformation  = 1,     // Undocument value
        ObjectTypeInformation  = 2
};

typedef struct __PUBLIC_OBJECT_TYPE_INFORMATION {
        UNICODE_STRING TypeName;
        ULONG          Reserved[22];
        ULONG          Reserved2[256];  // Add this field else the NtQueryObject
                                        // will fail for buffer size too small.
} PUBLIC_OBJECT_TYPE_INFORMATION, *PPUBLIC_OBJECT_TYPE_INFORMATION;

typedef NTSTATUS (WINAPI* _NtQuerySystemInformation)(
        ULONG SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength,
        PULONG ReturnLength
);

typedef 
NTSTATUS (WINAPI* _NtQueryObject)(
        HANDLE Handle,
        OBJECT_INFORMATION_CLASS ObjectInformationClass,
        PVOID ObjectInformation,
        ULONG ObjectInformationLength,
        PULONG ReturnLength
);

_NtQuerySystemInformation pNtQuerySystemInformation = NULL;
_NtQueryObject            pNtQueryObject            = NULL;


//-------------------------------------------------
//             User defined stuctures
//-------------------------------------------------
struct FileHolderInfo {
        vector<wstring>* file_list_;
        vector<ProcessInfo>* file_holder_;
};

typedef void (* EnumHandleCallback)(
        SYSTEM_HANDLE_INFORMATION* handle_list,
        void* arg
);


//-------------------------------------------------
//                Helper functions
//-------------------------------------------------
// Some handle will cause hang in the windows API, from internet some people
// find the handles with these flags may cause hang.
// This is undocumented and , there may have other flags that we don't know
// will cause hang.
bool MayCauseHang(ULONG access)
{
        switch (access) {
        case 0x120189:
        case 0x12019F:
        case 0x1a019f:
                return true;
        default:
                return false;
        }
}

bool GetNativeAPI()
{
        // Get the Native API address
        if (!pNtQueryObject || !pNtQuerySystemInformation) {
                FARPROC proc              = GetProcAddress2("ntdll.dll", "NtQuerySystemInformation");
                pNtQuerySystemInformation = reinterpret_cast<_NtQuerySystemInformation>(proc);
                proc                      = GetProcAddress2("ntdll.dll", "NtQueryObject");
                pNtQueryObject            = reinterpret_cast<_NtQueryObject>(proc);
        
                if (!pNtQuerySystemInformation || !pNtQueryObject) {
                        PrintErrorWith("Get Native API fail!");
                        return false;
                }
        }

        return true;
}

bool ObjectNameEqualTo(const WCHAR* file_name,
                       const WCHAR *object_name,
                       size_t obj_len)
{
        WCHAR buf[512] = {};

        if (!DrivePathToDevicePath(file_name, buf, _countof(buf)))
                return false;

        if (wcslen(buf) != obj_len)
                return false;

        return (0 == wcscmp(buf, object_name)) ? true : false;
}

bool ObjectIsFile(USHORT obj_type)
{
        // For object of file, ObjectTypeIndex is 28 in XP, 2000, and win7;
        // 25 in Vista.
 
        return (28 == obj_type || 25 == obj_type) ? true : false;
}

void DoFindFileHolder(SYSTEM_HANDLE_INFORMATION* handle_list, void* arg)
{
        for (ULONG_PTR i = 0; i < handle_list->num_of_handles_; i++) {
                SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle_info = handle_list->handles_[i];
       
                if (!ObjectIsFile(handle_info.ObjectTypeIndex))
                        continue;
                
                ULONG_PTR pid  = handle_info.UniqueProcessId;
                HANDLE process = OpenProcess(PROCESS_DUP_HANDLE |
                                             PROCESS_QUERY_INFORMATION,
                                             FALSE,
                                             pid);
                if (!process) {
                        printf("pid=%u\n", pid);
                        LOG("Open process fail!");
                        continue;
                }
               
                // We can't use those file handles directly because
                // they belong to other processes.
                // We must create a local handle in our process first.
                HANDLE dup_handle = NULL;
        
                BOOL result = DuplicateHandle(process, 
                                              handle_info.HandleValue,
                                              GetCurrentProcess(),
                                              &dup_handle,
                                              0,
                                              FALSE,
                                              0);
                if (!result) {
                        CloseHandle(process);
                        LOG("Duplicate handle fail!");
                        continue;
                }
                
                printf("pid=%u access=%x\n", pid, handle_info.GrantedAccess);
                
                DWORD file_type = 0;
        
                if (MayCauseHang(handle_info.GrantedAccess))
                        file_type = FILE_TYPE_UNKNOWN;
                else
                        file_type = GetFileType(dup_handle);
                
                LOG("after GetFileType");
                // If GetFileType fail, the return type will be FILE_TYPE_UNKNOWN
                // and GetLastError will not return NO_ERROR
                if (FILE_TYPE_UNKNOWN != file_type || NO_ERROR == GetLastError()) {
                        if (FILE_TYPE_DISK == file_type) {
                                PUBLIC_OBJECT_TYPE_INFORMATION type_info = {};
                                ULONG needed                             = 0;
        
                                NTSTATUS status = pNtQueryObject(dup_handle,
                                                                 ObjectNameInformation,
                                                                 &type_info,
                                                                 sizeof(type_info),
                                                                 &needed);
                                if (0 != status) {
                                        printf("NtQueryObject fail! status=%x needed=%u\n",
                                                status, needed);
                                } else {
                                        FileHolderInfo* holder_info =
                                                reinterpret_cast<FileHolderInfo*>(arg);
        
                                        vector<wstring>* file_list       = holder_info->file_list_;
                                        vector<ProcessInfo>* file_holder = holder_info->file_holder_;
                                        vector<wstring>::iterator it     = file_list->begin();
        
                                        for (; it != file_list->end(); it++) {
                                                // Be careful that the Langth field of UNICODE_STRING
                                                // specified length *in bytes*.
                                                USHORT str_len = type_info.TypeName.Length / 
                                                                 (sizeof(WCHAR) / sizeof(CHAR));
                                                
                                                PWSTR buffer   = type_info.TypeName.Buffer;
        
                                                if (!ObjectNameEqualTo(it->c_str(), buffer, str_len))
                                                        continue;

                                                ProcessInfo process_info = {
                                                        *it,
                                                        pid
                                                };

                                                file_holder->push_back(process_info);
                                        }
                               }
                        }
                } else {
                        LOG("GetFileType fail!");
                }
        
                CloseHandle(process);
                CloseHandle(dup_handle);
        }
}

void DoFindFileHolderByDriver(SYSTEM_HANDLE_INFORMATION* handle_list,
                              void* arg)
{
        HANDLE device = CreateFile(_T("\\\\.\\ObjNameGetter"),
                                   GENERIC_READ | GENERIC_WRITE,
                                   0,      // Not allow share
                                   NULL,   // No security
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);

        if (INVALID_HANDLE_VALUE == device) {
                PrintErrorWith("Create device fail!");
                return;
        }

        for (ULONG_PTR i = 0; i < handle_list->num_of_handles_; i++) {
                SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle_info = handle_list->handles_[i];

                if (!ObjectIsFile(handle_info.ObjectTypeIndex))
                        continue;

                void* addr       = handle_info.Object;
                WCHAR buf[512]   = {};
                DWORD bytes_read = 0;
                DWORD ctl_code   = CTL_CODE(FILE_DEVICE_UNKNOWN,
                                            2048,
                                            METHOD_BUFFERED,
                                            FILE_ANY_ACCESS);
                
                BOOL result = DeviceIoControl(device,
                                              ctl_code,
                                              &addr,
                                              sizeof(addr),
                                              buf,
                                              sizeof(buf),
                                              &bytes_read,
                                              NULL);
                if (!result) {
                        LOG("DeviceIoControl fail!");
                        continue;
                }

                FileHolderInfo* holder_info =
                        reinterpret_cast<FileHolderInfo*>(arg);

                vector<wstring>* file_list       = holder_info->file_list_;
                vector<ProcessInfo>* file_holder = holder_info->file_holder_;
                vector<wstring>::iterator it     = file_list->begin();

                size_t len = bytes_read / sizeof(WCHAR);

                if (bytes_read == sizeof(buf)) {
                        buf[len - 1] = _T('\0');
                        puts("Object name exceed limit!");
                        --len;
                }

                for (; it != file_list->end(); it++) {
                        if (!ObjectNameEqualTo(it->c_str(), buf, len))
                                continue;
 
                        ProcessInfo process_info = {
                                *it,
                                handle_info.UniqueProcessId
                        };
                        
                        file_holder->push_back(process_info);
                }
        }

        CloseHandle(device);
        return;
}

void EnumHandleList(EnumHandleCallback callback, void* arg)
{
        // We don't know how many size needed to get the system handle information,
        // so use a loop and increase the buffer size every time.
        DWORD needed                           = 0;
        SYSTEM_HANDLE_INFORMATION* handle_list = NULL;

        for (int i = 0; i < INT_MAX ; i++) {
                // 64 is an undocumented value, it stands for
                // SystemExtendedHandleInformation and can be used
                // to get system handle list.
                SYSTEM_INFORMATION_CLASS HANDLE_INFO_EX = 
                        static_cast<SYSTEM_INFORMATION_CLASS>(64);

                size_t buf_size = sizeof(SYSTEM_HANDLE_INFORMATION) +
                                  needed + (1024 * i);

                handle_list = reinterpret_cast<SYSTEM_HANDLE_INFORMATION*>(new BYTE[buf_size]);
                
                // Get the all opened handle in the system
                NTSTATUS status = pNtQuerySystemInformation(HANDLE_INFO_EX,
                                                            handle_list,
                                                            buf_size,
                                                            &needed);
                if (0 != status) {
                        delete [] handle_list;

                        if (0 == needed) {
                                // Some other error occur, most probably we don't
                                // have the access right.
                                printf("status=%x\n", status);
                                LOG("query system info error!");
                                return;
                        } else {
                                // The buffer size is not big enough.
                                continue;
                        }
                }
                break;
        }
        
        // Get the system handle list successfully, now we can call the
        // callback function
        callback(handle_list, arg);
       
        delete [] handle_list;
}

}       // End of unnamed namespace


// The file_path must be a fully qualified path
bool FindFileHolder(vector<wstring>* file_list, vector<ProcessInfo>* file_holder)
{
        if (!GetNativeAPI()) return false;
        
        FileHolderInfo holder_info = {file_list, file_holder};

        EnumHandleList(DoFindFileHolder, &holder_info);
        return true;
}

bool FindFileHolderByDriver(vector<wstring>* file_list,
                            vector<ProcessInfo>* file_holder)
{
        if (!GetNativeAPI()) return false;
        
        FileHolderInfo holder_info = {file_list, file_holder};

        EnumHandleList(DoFindFileHolderByDriver, &holder_info);
        return true;
}

bool FindDllHolder(const WCHAR* dll_path, vector<ProcessInfo>* file_holder)
{
        DWORD* pid_list      = NULL;
        DWORD bytes_returned = 0;

        for (int i = 1; i < INT_MAX; i++) {
                DWORD list_size = 1024 * i;
                pid_list        = new DWORD[list_size];
                bytes_returned  = 0;
                
                // Make sure we the size of buffer we allocate is big
                // enough for the process list.
                EnumProcesses(pid_list, list_size, &bytes_returned);
                if (bytes_returned  >= list_size) {
                        delete [] pid_list;
                        continue;
                }
                break;
        }
                
        // Enumerate all modules loaded by the process
        DWORD num_pid = bytes_returned / sizeof(DWORD);

        for (DWORD j = 0; j < num_pid; j++) {
                HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
                                                            pid_list[j]);
                if (INVALID_HANDLE_VALUE == snap_shot) {
                        PrintErrorWith("CreateToolhelp32Snapshot fail!");
                        continue;
                }
                
                MODULEENTRY32 module_info = {sizeof(module_info)};

                if (!Module32FirstW(snap_shot, &module_info)) {
                        PrintErrorWith("Module32First fail!");
                        CloseHandle(snap_shot);
                        continue;
                }
                
                // The first module is the executable, we keep it.
                do {
                        size_t str_len = wcslen(module_info.szExePath);

                        if (str_len != wcslen(dll_path)) continue;

                        if (0 == lstrcmpiW(module_info.szExePath, dll_path)) {
                                ProcessInfo process_info = {
                                        dll_path,
                                        module_info.th32ProcessID
                                };

                                file_holder->push_back(process_info);
                                break;
                        }
                } while (Module32NextW(snap_shot, &module_info));
                
                CloseHandle(snap_shot);
        }

        delete [] pid_list;
        return true;
}

bool GetDebugPrivilege()
{
        HANDLE token = NULL;

        if (!OpenProcessToken(GetCurrentProcess(),
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                              &token)) {
                PrintErrorWith("OpenProcessToken fail!");
                return false;
        }
        
        if (!SetPrivilege(token, SE_DEBUG_NAME, true))
                return false;

        CloseHandle(token);
        return true;
}

void TryGetDebugPrivilege()
{
        if (!GetDebugPrivilege())
                puts("get debug privilege fail!");
        else
                puts("get debug privilege success!");
}
