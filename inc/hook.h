#ifndef HOOK_H
#define HOOK_H

#ifdef BUILDDLL
        #define DLLAPI __declspec(dllexport)
#else
        #define DLLAPI __declspec(dllimport)
#endif

#include <Windows.h>

DLLAPI void ReplaceIATEntryInOneModule(const char* callee_module_name,
                                       FARPROC fn_current,
                                       FARPROC fn_new,
                                       HMODULE caller_module);

DLLAPI void ReplaceIATEntryInAllModules(const char* callee_module_name,
                                        FARPROC fn_current,
                                        FARPROC fn_new);

DLLAPI void ReplaceDelayIATEntryInOneModule(const char* callee_module_name,
                                            FARPROC fn_origin,
                                            FARPROC fn_hook,
                                            HMODULE module);

DLLAPI void ReplaceDelayIATEntryInAllModules(const char* callee_module_name,
                                             FARPROC fn_origin,
                                             FARPROC fn_hook);

DLLAPI void InitHookModule(HINSTANCE module);
DLLAPI void DeinitHookModule();

DLLAPI void APIHook(const char* callee_module_name,
                    const char* fn_name,
                    FARPROC fn_hook);

DLLAPI void APIUnhook(const char* callee_module_name, const char* fn_name);

DLLAPI void DelayAPIHook(const char* callee_module_name,
                         const char* fn_name,
                         FARPROC fn_hook);

DLLAPI void DelayAPIUnhook(const char* callee_module_name, const char* fn_name);

#endif  // End of header guard
