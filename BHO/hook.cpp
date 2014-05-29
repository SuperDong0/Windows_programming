#define _CRT_SECURE_NO_WARNINGS

#define NDEBUG

#include "hook.h"
#include "helper.h"
#include "com_helper.h"
#include <cstdio>
#include <string>
#include <list>
#include <Dbghelp.h>
#include <TlHelp32.h>
#include <Shlwapi.h>
#include <DelayImp.h>

using std::list;
using std::string;

namespace {

struct HookInfo {
        HookInfo(const char* module_name,
                 const char* fn_name,
                 FARPROC fn_origin,
                 FARPROC fn_hook)
                : module_name_(module_name)
                , fn_name_(fn_name)
                , fn_origin_(fn_origin)
                , fn_hook_(fn_hook)
        {
        }

        string module_name_;
        string fn_name_;
        FARPROC fn_origin_;
        FARPROC fn_hook_;
};

HINSTANCE g_instance = NULL;

// It's important that we use stl list instead of vector.
// IE may call LoadLibrary/GetProcAddress APIs which hooked by us 
// and the hook function will iterate through the global
// lists in other threads during we use push_back/erase
// to modify the global lists.
// If using vector, it is very likely that the iterator will
// become invalid if other threads invoke push_back/erase.
// if using list, the iterator will still valid if other threads
// invoke push_back at the same time.
// But the erase method still has possibility that makes the
// iterator invalid and cause host process crash
list<HookInfo> g_hook_list;
list<HookInfo> g_delay_hook_list;

//-------------------------------------------------
//               Helper functions 
//-------------------------------------------------
void WriteProcessMemory2(HANDLE hProcess,
                         LPVOID lpBaseAddress,
                         LPCVOID lpBuffer,
                         SIZE_T nSize,
                         SIZE_T* lpNumberOfBytesWritten)
{
        BOOL result = WriteProcessMemory(hProcess,
                                         lpBaseAddress,
                                         lpBuffer,
                                         nSize,
                                         lpNumberOfBytesWritten);
         if (result) return;

         if (ERROR_NOACCESS == GetLastError()) {
                 DWORD old_protect_flag = 0;
                 
                if (VirtualProtect(lpBaseAddress,
                                   nSize,
                                   PAGE_WRITECOPY,
                                   &old_protect_flag)) {

                        if (!WriteProcessMemory(hProcess,
                                                lpBaseAddress,
                                                lpBuffer,
                                                nSize,
                                                lpNumberOfBytesWritten)) {
                                LOG("WriteProcessMemory fail! code=%u\n", GetLastError());
                        }

                        VirtualProtect(lpBaseAddress,
                                       nSize,
                                       old_protect_flag,
                                       &old_protect_flag);
                } else {
                        LOG("VirtualProtect fail!\n");
                }
         } else {
                 LOG("WriteProcessMemory fail!\n");
         }
}

void DoHookLoadLibrary(HMODULE module)
{
        if (module == g_instance) return;
        
        list<HookInfo>::iterator it = g_hook_list.begin();

        for (; it != g_hook_list.end(); it++) {
                ReplaceIATEntryInAllModules(it->module_name_.c_str(),
                                            it->fn_origin_,
                                            it->fn_hook_);
        }

        it = g_delay_hook_list.begin();

        for (; it != g_delay_hook_list.end(); it++) {
                ReplaceDelayIATEntryInAllModules(it->module_name_.c_str(),
                                                 it->fn_origin_,
                                                 it->fn_hook_);
        }
}

void DoHookLoadLibraryEx(HMODULE module, DWORD dwFlags)
{
        if (!module || module == g_instance) return;

        if (((dwFlags & LOAD_LIBRARY_AS_DATAFILE) != 0) ||
            ((dwFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) != 0) ||
            ((dwFlags & LOAD_LIBRARY_AS_IMAGE_RESOURCE) != 0))
                return;
        
        DoHookLoadLibrary(module);
}


//-------------------------------------------------
//        Hook functions for kernel32 API
//-------------------------------------------------
HMODULE WINAPI HookLoadLibraryA(LPCSTR lpFileName)
{
        HMODULE module = LoadLibraryA(lpFileName);

        DoHookLoadLibrary(module);
        return module;
}

HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName)
{
        HMODULE module = LoadLibraryW(lpFileName);

        DoHookLoadLibrary(module);
        return module;
}

HMODULE WINAPI HookLoadLibraryExA(LPCSTR lpLibFileName,
                                  HANDLE hFile,
                                  DWORD dwFlags)
{
        HMODULE module = LoadLibraryExA(lpLibFileName,
                                        hFile,
                                        dwFlags);
        DoHookLoadLibraryEx(module, dwFlags);
        return module;
}

HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpLibFileName,
                                  HANDLE hFile,
                                  DWORD dwFlags)
{
        HMODULE module = LoadLibraryExW(lpLibFileName,
                                        hFile,
                                        dwFlags);
        DoHookLoadLibraryEx(module, dwFlags);
        return module;
}


FARPROC WINAPI HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
        FARPROC proc = GetProcAddress(hModule, lpProcName);
        
        list<HookInfo>::iterator it = g_hook_list.begin();
        for (; it != g_hook_list.end(); it++) {
                if (proc == it->fn_origin_) {
                        LOG("HookGetProcAddress for %s\n", it->fn_name_.c_str());
                        return it->fn_hook_;
                }
        }

        it = g_delay_hook_list.begin();
        for (; it != g_delay_hook_list.end(); it++) {
                if (proc == it->fn_origin_) {
                        LOG("HookGetProcAddress for %s\n", it->fn_name_.c_str());
                        return it->fn_hook_;
                }
        }
        
        return proc;
}


}       // End of unnamed namespace



void ReplaceIATEntryInOneModule(const char* callee_module_name,
                                FARPROC fn_origin,
                                FARPROC fn_hook,
                                HMODULE caller_module)
{
        IMAGE_IMPORT_DESCRIPTOR* import_desctiptor = NULL;
        ULONG data_size                            = 0;
        
        // Get the image header information
        import_desctiptor = (IMAGE_IMPORT_DESCRIPTOR*)
                            ImageDirectoryEntryToData(caller_module,
                                                      TRUE,
                                                      IMAGE_DIRECTORY_ENTRY_IMPORT,
                                                      &data_size);
        if (!import_desctiptor) {
                //LOG("ImageDirectoryEntryToData fail! code=%u\n", GetLastError());
                return;
        }
        
        // Find module to replace
        for (; import_desctiptor->Name; import_desctiptor++) {
                CHAR* module_name = (CHAR*)caller_module + import_desctiptor->Name;
                
                // Use case-insensitive function to compare string
                if (0 == lstrcmpiA(module_name, callee_module_name))
                        break;
        }

        if (!import_desctiptor->Name) return;
        
        // FirstThunk point to IAT.
        // Find function entry in IAT to replace
        IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)
                                  ((BYTE*)caller_module + import_desctiptor->FirstThunk);
        
        for (; thunk->u1.Function; thunk++) {
                FARPROC* pfn = (FARPROC*)&thunk->u1.Function;

                if (*pfn == fn_origin) {
                        WriteProcessMemory2(GetCurrentProcess(),
                                            pfn,
                                            &fn_hook,
                                            sizeof(fn_hook),
                                            NULL);
                        break;
                }
        }
}

void ReplaceIATEntryInAllModules(const char* callee_module_name,
                                 FARPROC fn_origin,
                                 FARPROC fn_hook)
{
        // If we did not get the module handle, we can't perform the hook action.
        if (!g_instance) return;

        HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
                                                    GetCurrentProcessId());

        if (INVALID_HANDLE_VALUE == snap_shot) {
                LOG("CreateToolhelp32Snapshot fail!\n");
                return;
        }

        MODULEENTRY32 module_info = {sizeof(module_info)};

        if (!Module32First(snap_shot, &module_info)) {
                LOG("Module32First fail!\n");
                CloseHandle(snap_shot);
                return;
        }

        do {
                // We do not hook API in our own module, so that any API
                // function call will use the read API instead of hooked
                // function in this module.
                if (g_instance == module_info.hModule) continue;
                
                // Don't know why but ImageDirectoryEntryToData API will
                // fail for ntdll.dll, so we just skip it now.
                // But it should be OK because ntdll.dll doesn't import any
                // functions.
                if (0 == lstrcmpi(module_info.szExePath,
                                  _T("C:\\WINDOWS\\system32\\ntdll.dll"))) continue;
                
                // Some program will use other module such as .drv, so we need
                // check this.
                TCHAR* ext = PathFindExtension(module_info.szModule);
                if (0 != lstrcmpi(ext, _T(".dll")) &&
                    0 != lstrcmpi(ext, _T(".exe")))
                        continue;

                ReplaceIATEntryInOneModule(callee_module_name,
                                           fn_origin,
                                           fn_hook,
                                           module_info.hModule);
        } while (Module32Next(snap_shot, &module_info));

        CloseHandle(snap_shot);
}

void ReplaceDelayIATEntryInOneModule(const char* callee_module_name,
                                     FARPROC fn_origin,
                                     FARPROC fn_hook,
                                     HMODULE module)
{
        DWORD size                      = 0;  
        ImgDelayDescr* delay_descriptor = NULL;
        char* base_addr                 = reinterpret_cast<char*>(module);
        
        delay_descriptor = (ImgDelayDescr*)
                           ImageDirectoryEntryToData(module,
                                                     TRUE,
                                                     IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT,
                                                     &size);
        if (!delay_descriptor)
                return;

        for (; delay_descriptor->rvaDLLName; delay_descriptor++) {
                char* delay_dll = base_addr + delay_descriptor->rvaDLLName;

                if (0 == lstrcmpiA(callee_module_name, delay_dll))
                        break;
        }

        if (!delay_descriptor->rvaDLLName) return;
        if (0 == (delay_descriptor->grAttrs & dlattrRva)) {
                LOG("delay structure use virtual address, not RVA!\n");
                return;
        }
        // FirstThunk point to IAT.
        // Find function entry in IAT to replace
        IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)
                                  (base_addr + delay_descriptor->rvaIAT);
        
        for (; thunk->u1.Function; thunk++) {
                FARPROC* pfn = (FARPROC*)&thunk->u1.Function;

                if (*pfn == fn_origin) {
                        WriteProcessMemory2(GetCurrentProcess(),
                                            pfn,
                                            &fn_hook,
                                            sizeof(fn_hook),
                                            NULL);
                        break;
                }
        }
}

void ReplaceDelayIATEntryInAllModules(const char* callee_module_name,
                                      FARPROC fn_origin,
                                      FARPROC fn_hook)
{
        // If we did not get the module handle, we can't perform the hook action.
        if (!g_instance) return;

        HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
                                                    GetCurrentProcessId());
        if (INVALID_HANDLE_VALUE == snap_shot) {
                LOG("CreateToolhelp32Snapshot fail!\n");
                return;
        }

        MODULEENTRY32 module_info = {sizeof(module_info)};

        if (!Module32First(snap_shot, &module_info)) {
                LOG("Module32First fail!\n");
                CloseHandle(snap_shot);
                return;
        }

        do {
                // We do not hook API in our own module, so that any API
                // function call will use the read API instead of hooked
                // function in this module.
                if (g_instance == module_info.hModule) continue;
                
                // Don't know why but ImageDirectoryEntryToData API will
                // fail for ntdll.dll, so we just skip it now.
                // But it should be OK because ntdll.dll doesn't import any
                // functions.
                if (0 == lstrcmpi(module_info.szExePath,
                                  _T("C:\\WINDOWS\\system32\\ntdll.dll"))) continue;
                
                // Some program will use other module such as .drv, so we need
                // check this.
                TCHAR* ext = PathFindExtension(module_info.szModule);
                if (0 != lstrcmpi(ext, _T(".dll")) &&
                    0 != lstrcmpi(ext, _T(".exe")))
                        continue;

                ReplaceDelayIATEntryInOneModule(callee_module_name,
                                                fn_origin,
                                                fn_hook,
                                                module_info.hModule);
        } while (Module32Next(snap_shot, &module_info));

        CloseHandle(snap_shot);
}

void InitHookModule(HINSTANCE module)
{
        g_instance = module;

        // Hook all these API so any dynamic loaded library will be hooked
        // after it loaded.
        // Also add these hooks to the global hook list.
        APIHook("kernel32.dll", "LoadLibraryA", (FARPROC)HookLoadLibraryA);
        APIHook("kernel32.dll", "LoadLibraryW", (FARPROC)HookLoadLibraryW);
        APIHook("kernel32.dll", "LoadLibraryExA", (FARPROC)HookLoadLibraryExA);
        APIHook("kernel32.dll", "LoadLibraryExW", (FARPROC)HookLoadLibraryExW);
        APIHook("kernel32.dll", "GetProcAddress", (FARPROC)HookGetProcAddress);
}

void DeinitHookModule()
{
        if (!g_instance) return;

        {
                list<HookInfo>::iterator it = g_hook_list.begin();
        
                for (; it != g_hook_list.end(); it++) {
                        ReplaceIATEntryInAllModules(it->module_name_.c_str(),
                                                    it->fn_hook_,
                                                    it->fn_origin_);
                }
                g_hook_list.clear();
        
                it = g_delay_hook_list.begin();
        
                for (; it != g_delay_hook_list.end(); it++) {
                        ReplaceDelayIATEntryInAllModules(it->module_name_.c_str(),
                                                         it->fn_hook_,
                                                         it->fn_origin_);
                }
                g_delay_hook_list.clear();
        }
}

void APIHook(const char* callee_module_name,
             const char* fn_name,
             FARPROC fn_hook)
{
        FARPROC fn_origin = GetProcAddress(GetModuleHandleA(callee_module_name),
                                           fn_name);
        if (fn_origin) {
                ReplaceIATEntryInAllModules(callee_module_name, fn_origin, fn_hook);

                HookInfo info(callee_module_name, fn_name, fn_origin, fn_hook);

                g_hook_list.push_back(info);
        } else {
                LOG("hook fail, %s may not loaded.\n", callee_module_name);
        }
}

void APIUnhook(const char* callee_module_name, const char* fn_name)
{
        FARPROC fn_origin = GetProcAddress(GetModuleHandleA(callee_module_name),
                                           fn_name);

        list<HookInfo>::iterator it = g_hook_list.begin();

        for (; it != g_hook_list.end(); it++) {
                if (it->fn_origin_ == fn_origin) {
                        ReplaceIATEntryInAllModules(callee_module_name,
                                                    it->fn_hook_,
                                                    fn_origin);
                        g_hook_list.erase(it);
                        break;
                }
        }
}

void DelayAPIHook(const char* callee_module_name,
                  const char* fn_name,
                  FARPROC fn_hook)
{
        FARPROC fn_origin = GetProcAddress(GetModuleHandleA(callee_module_name),
                                           fn_name);
        if (fn_origin) {
                ReplaceDelayIATEntryInAllModules(callee_module_name,
                                                 fn_origin,
                                                 fn_hook);

                HookInfo info(callee_module_name, fn_name, fn_origin, fn_hook);
                
                g_delay_hook_list.push_back(info);
        } else {
                LOG("hook fail, %s may not loaded.\n", callee_module_name);
        }
}

void DelayAPIUnhook(const char* callee_module_name, const char* fn_name)
{
        FARPROC fn_origin = GetProcAddress(GetModuleHandleA(callee_module_name),
                                           fn_name);

        
        list<HookInfo>::iterator it = g_delay_hook_list.begin();

        for (; it != g_delay_hook_list.end(); it++) {
                if (it->fn_origin_ == fn_origin) {
                        ReplaceDelayIATEntryInAllModules(callee_module_name,
                                                         it->fn_hook_,
                                                         fn_origin);
                        g_delay_hook_list.erase(it);
                        break;
                }
        }
}
