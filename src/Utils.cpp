//
//  rtmp_relay
//

#if defined(__APPLE__) || defined(__linux__)
#include <sys/syslog.h>
#elif defined(_MSC_VER)
#define NOMINMAX
#include <windows.h>
#include <strsafe.h>
#endif

#include "Utils.h"

static char TEMP_BUFFER[1024];

void log(LogLevel level, const char* format, ...)
{
    va_list list;
    va_start(list, format);

    vsprintf(TEMP_BUFFER, format, list);

    va_end(list);

#if defined(__APPLE__) || defined(__linux__)
    printf("%s\n", TEMP_BUFFER);
    int prio = 0;
    switch (level)
    {
        case LOG_LEVEL_ERROR: prio = LOG_ERR; break;
        case LOG_LEVEL_WARNING: prio = LOG_WARNING; break;
        case LOG_LEVEL_INFO: prio = LOG_INFO; break;
        case LOG_LEVEL_VERBOSE: prio = LOG_DEBUG; break;
    }
    syslog(prio, "%s", TEMP_BUFFER);
    printf("%s\n", TEMP_BUFFER);
#elif defined(_MSC_VER)
    wchar_t szBuffer[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, TEMP_BUFFER, -1, szBuffer, MAX_PATH);
    StringCchCat(szBuffer, sizeof(szBuffer), L"\n");
    OutputDebugString(szBuffer);
#endif
}
