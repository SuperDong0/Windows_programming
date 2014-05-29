#define _CRT_SECURE_NO_WARNINGS

#include "log.h"
#include <cstdio>
#include <cstdarg>

namespace {

const char* g_log_name           = "log.txt";
char        g_log_path[MAX_PATH] = {};

}

void InitializeLogPath()
{
        GetCurrentDirectory(sizeof(g_log_path), g_log_path);

        strncat(g_log_path, "\\", MAX_PATH - strlen(g_log_path) - 1);
        strncat(g_log_path, g_log_name, MAX_PATH - strlen(g_log_path) - 1);
}

void Log(const char* msg, ...)
{
        char buf[256] = {};

        va_list args;
        va_start(args, msg);
        vsprintf(buf, msg, args);
        va_end(args);

        FILE* fp = fopen(g_log_path, "a");

        fprintf(fp, buf);
        fclose(fp);
}
