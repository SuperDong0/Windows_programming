#ifndef LOG_H
#define LOG_H

#include <windows.h>

void InitializeLogPath();
void Log(const char* msg, ...);

#endif  // End of header guard
