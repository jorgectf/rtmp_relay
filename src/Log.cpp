//
//  rtmp_relay
//

#include <iostream>
#include <string>
#include <ctime>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>
#else
    #if defined(LOG_SYSLOG)
    #include <sys/syslog.h>
    #endif
#endif
#include "Log.hpp"

namespace relay
{
#ifndef DEBUG
    Log::Level Log::threshold = Log::Level::INFO;
#else
    Log::Level Log::threshold = Log::Level::ALL;
#endif

#if defined(LOG_SYSLOG)
    bool Log::syslogEnabled = true;
#else
    bool Log::syslogEnabled = false;
#endif

    void Log::flush()
    {
        if (!s.empty())
        {
            char timeBuffer[26];
            int millisec;
            struct tm* tm_info;
            struct timeval tv;

            gettimeofday(&tv, NULL);

            millisec = lrint(tv.tv_usec / 1000.0); // Round to nearest millisec
            if (millisec >= 1000) { // Allow for rounding up to nearest second
                millisec -=1000;
                tv.tv_sec++;
            }

            tm_info = localtime(&tv.tv_sec);

            strftime(timeBuffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
            sprintf(&timeBuffer[19], ".%03d", millisec);

            if (level == Level::ERR ||
                level == Level::WARN)
            {
                std::cerr << timeBuffer << ": " << s << std::endl;
            }
            else
            {
                std::cout << timeBuffer << ": " << s << std::endl;
            }

#ifdef _WIN32
            wchar_t szBuffer[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, szBuffer, MAX_PATH);
            StringCchCatW(szBuffer, sizeof(szBuffer), L"\n");
            OutputDebugStringW(szBuffer);
#elif defined(LOG_SYSLOG)
            if (syslogEnabled)
            {
                int priority = 0;
                switch (level)
                {
                    case Level::ERR: priority = LOG_ERR; break;
                    case Level::WARN: priority = LOG_WARNING; break;
                    case Level::INFO: priority = LOG_INFO; break;
                    case Level::ALL: priority = LOG_DEBUG; break;
                    default: break;
                }
                syslog(priority, "%s", s.c_str());
            }
#endif
            s.clear();
        }
    }
}
