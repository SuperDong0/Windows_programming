#define _CRT_SECURE_NO_WARNINGS

#include "com_helper.h"
#include <cassert>

// Important! Every functions declared in com_helper.h header should also
// be add to this section.
// If NDEBUG not defined, the function name will be replaced to empty string.
// So we should undefine the macro else will get compile error.
#ifndef NDEBUG

#include <cstdio>
#include <tchar.h>
#include <Unknwn.h>

#undef InitLogFile
#undef PrintIID
#undef CloseLogFile
#undef LOG

#endif

FILE* g_file = NULL;

void InitLogFile(const char* log_path)
{
        assert(log_path && "log path cannot be NULL!");

        g_file = fopen(log_path, "w");
}

void PrintIID(const TCHAR* prefix, REFIID riid)
{
        LPOLESTR iid   = NULL;
        HRESULT result = StringFromIID(riid, &iid);

        if (S_OK == result) {
                fwprintf(g_file, L"%ls iid=%ls\n", prefix, iid);
                fflush(g_file);
                CoTaskMemFree(iid);
        }
}

void CloseLogFile()
{
        if (g_file)
                fclose(g_file);
}
