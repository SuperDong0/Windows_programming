#ifndef ENUM_OPEN_FILES_H
#define ENUM_OPEN_FILES_H

#include <tchar.h>
#include <windows.h>
#include <vector>
#include <string>

struct ProcessInfo {
        std::wstring file_path_;
        ULONG_PTR pid_;
};

bool FindFileHolder(std::vector<std::wstring>* file_list,
                    std::vector<ProcessInfo>* file_holder);

bool FindDllHolder(const WCHAR* dll_path, std::vector<ProcessInfo>* file_holder);
bool GetDebugPrivilege();
void TryGetDebugPrivilege();

bool FindFileHolderByDriver(std::vector<std::wstring>* file_list,
                            std::vector<ProcessInfo>* file_holder);

#endif  // End of header guard
