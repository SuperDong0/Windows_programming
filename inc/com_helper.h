#ifndef COM_HELPER_H
#define COM_HELPER_H

#ifdef NDEBUG
#include <cstdio>
#include <tchar.h>
#include <Unknwn.h>

extern FILE* g_file;

void InitLogFile(const char* log_path);
void PrintIID(const TCHAR* prefix, REFIID riid);
void CloseLogFile();

#define LOG(a, ...) \
        do {\
                fprintf(g_file, a, __VA_ARGS__); \
                fflush(g_file); \
        } while (stdout == NULL)        // Use a trick to disable warning C4127

#else

#define InitLogFile(a)
#define PrintIID(a, b)
#define LOG(...)
#define CloseLogFile()

#endif

#endif  // End of header guard
