//
//  rtmp_relay
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>

#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>

#include "Relay.h"
#include "Log.h"

using namespace relay;
using namespace cppsocket;

static std::string config;
cppsocket::Network network;
Relay rel(network);

#ifndef _MSC_VER
static void signalHandler(int signo)
{
    switch(signo)
    {
        case SIGHUP:
            // rehash the server
            if (!rel.init(config))
            {
                Log(Log::Level::ERR) << "Failed to reload config";
                exit(EXIT_FAILURE);
            }
            break;
        case SIGTERM:
            // shutdown the server
            exit(EXIT_SUCCESS);
            break;
        case SIGUSR1:
            rel.printInfo();
            break;
        case SIGPIPE:
            Log(Log::Level::ERR) << "Received SIGPIPE";
            break;
    }
}

static int daemonize(const char* lockFile)
{
    // drop to having init() as parent
    int i, pid = fork();
    char str[256] = {0};
    if (pid < 0)
    {
        Log(Log::Level::ERR) << "Failed to fork process";
        exit(EXIT_FAILURE);
    }
    if (pid > 0) exit(EXIT_SUCCESS); // parent process

    setsid();

    for (i = getdtablesize(); i>=0; i--)
        close(i);

    i = open("/dev/null", O_RDWR);
    dup(i); // stdout
    dup(i); // stderr
    umask(027);

    int lfp = open(lockFile, O_RDWR|O_CREAT|O_EXCL, 0640);

    if (lfp < 0)
    {
        Log(Log::Level::ERR) << "Failed to open lock file";
        exit(EXIT_FAILURE);
    }

    if (lockf(lfp, F_TLOCK, 0) < 0)
    {
        Log(Log::Level::ERR) << "Failed to lock the file";
        exit(EXIT_SUCCESS);
    }

    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); // record pid to lockfile

    // ignore child terminate signal
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to ignore SIGCHLD";
        exit(EXIT_FAILURE);
    }

    // hangup signal
    if (signal(SIGHUP, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGHUP";
        exit(EXIT_FAILURE);
    }

    // software termination signal from kill
    if (signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGTERM";
        exit(EXIT_FAILURE);
    }

    Log(Log::Level::INFO) << "Daemon started, pid: " << getpid();

    return EXIT_SUCCESS;
}

static int killDaemon(const char* lockFile)
{
    char pidStr[11];
    memset(pidStr, 0, sizeof(pidStr));

    int lfp = open(lockFile, O_RDONLY);

    if (lfp == -1)
    {
        Log(Log::Level::ERR) << "Failed to open lock file";
        return 0;
    }

    read(lfp, pidStr, sizeof(pidStr));

    pid_t pid = atoi(pidStr);

    if (kill(pid, SIGTERM) != 0)
    {
        Log(Log::Level::ERR) << "Failed to kill daemon";
        return 0;
    }

    close(lfp);

    Log(Log::Level::INFO) << "Daemon killed";

    return pid;
}
#endif

int main(int argc, const char* argv[])
{
    bool daemon = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--config") == 0)
        {
            if (++i < argc) config = argv[i];
        }
        else if (strcmp(argv[i], "--daemon") == 0)
        {
            daemon = true;
        }
        else if (strcmp(argv[i], "--kill-daemon") == 0)
        {
#ifndef _MSC_VER
            if (killDaemon("/var/run/rtmp_relay.pid"))
            {
                return EXIT_SUCCESS;
            }
            else
            {
                return EXIT_FAILURE;
            }
#endif
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            const char* exe = argc >= 1 ? argv[0] : "rtmp_relay";
            Log(Log::Level::INFO) << "Usage: " << exe << " --config <path to config file>";
            return EXIT_SUCCESS;
        }
    }

    if (config.empty())
    {
        Log(Log::Level::ERR) << "No config file";
        return EXIT_FAILURE;
    }

    if (daemon)
    {
#ifdef _MSC_VER
        Log(Log::Level::ERR) << "Daemon not supported on Windows";
        return EXIT_FAILURE;
#else
        if (daemonize("/var/run/rtmp_relay.pid") == -1) return EXIT_FAILURE;
#endif
    }

#ifndef _MSC_VER
    if (signal(SIGUSR1, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGUSR1";
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGPIPE";
        return EXIT_FAILURE;
    }
#endif
    
    if (!rel.init(config))
    {
        Log(Log::Level::ERR) << "Failed to init relay";
        return EXIT_FAILURE;
    }
    
    rel.run();
    
    return EXIT_SUCCESS;
}
