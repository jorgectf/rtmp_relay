//
//  rtmp_relay
//

#include <cstdlib>
#include <iostream>
#include <csignal>

#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>

#include "Constants.hpp"
#include "Relay.hpp"
#include "Log.hpp"

using namespace relay;
using namespace cppsocket;

static std::string config;
cppsocket::Network network;
Relay rel(network);

#ifndef _WIN32
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
            rel.close();
            rel.closeLog();
            exit(EXIT_SUCCESS);
            break;
        case SIGUSR1:
        {
            std::string str;
            rel.getStats(str, ReportType::TEXT);
            Log(Log::Level::INFO) << str;
            break;
        }
        case SIGPIPE:
            Log(Log::Level::ERR) << "Received SIGPIPE";
            break;
    }
}

static bool daemonize(const char* lock_file)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        Log(Log::Level::ERR) << "Failed to fork process";
        return false;
    }
    if (pid > 0) exit(EXIT_SUCCESS); // parent process

    rel.openLog();

    pid_t sid = setsid();

    if (sid < 0)
    {
        Log(Log::Level::ERR) << "Failed to create a session";
        return false;
    }

    // close all open file descriptors
    for (int i = getdtablesize(); i >= 0; --i)
    {
        close(i);
    }

    // redirect stdout and stderr to /dev/null
    int i = open("/dev/null", O_WRONLY);
    dup2(i, STDOUT_FILENO);
    dup2(i, STDERR_FILENO);

    // prevent access to created files to other users
    umask(027);

    int lfp = open(lock_file, O_RDWR|O_CREAT, 0600);

    if (lfp == -1)
    {
        Log(Log::Level::ERR) << "Failed to open lock file";
        return false;
    }

    if (lockf(lfp, F_TLOCK, 0) == -1)
    {
        Log(Log::Level::ERR) << "Failed to lock the file";
        return false;
    }

    std::string str = std::to_string(getpid());

    // record pid to lockfile
    if (write(lfp, str.c_str(), str.length()) == -1)
    {
        Log(Log::Level::ERR) << "Failed to write pid to lock file";
        return false;
    }

    // ignore child terminate signal
    if (std::signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to ignore SIGCHLD";
        return false;
    }

    // hangup signal
    if (std::signal(SIGHUP, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGHUP";
        return false;
    }

    Log(Log::Level::INFO) << "Daemon started, pid: " << getpid();

    return true;
}

static int getPid(const char* lockFile)
{
    int lfp = open(lockFile, O_RDONLY);

    if (lfp == -1)
    {
        Log(Log::Level::ERR) << "Failed to open lock file";
        return 0;
    }

    char str[20];
    if (read(lfp, str, sizeof(str)) == -1)
    {
        Log(Log::Level::ERR) << "Failed to read pid from the lock file";
        return 0;
    }

    pid_t pid = atoi(str);

    close(lfp);

    return pid;
}
#endif

int main(int argc, const char* argv[])
{
    Log(Log::Level::INFO) << "-----------------  RTMP Relay  -----------------";
    bool daemon = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--config")
        {
            if (++i < argc) config = argv[i];
        }
        else if (std::string(argv[i]) == "--reload-config")
        {
#ifndef _WIN32
            if (int pid = getPid("/var/run/rtmp_relay.pid"))
            {
                if (kill(pid, SIGHUP) != 0)
                {
                    Log(Log::Level::ERR) << "Failed to send SIGHUP to the daemon";
                    return EXIT_FAILURE;
                }

                return EXIT_SUCCESS;
            }
            else
            {
                Log(Log::Level::ERR) << "Failed to get the pid of the daemon";
                return EXIT_FAILURE;
            }
#else
            Log(Log::Level::ERR) << "Daemon is not supported on Windows";
            return EXIT_FAILURE;
#endif
        }
        else if (std::string(argv[i]) == "--daemon")
        {
            daemon = true;
        }
        else if (std::string(argv[i]) == "--kill-daemon")
        {
#ifndef _WIN32
            if (int pid = getPid("/var/run/rtmp_relay.pid"))
            {
                if (unlink("/var/run/rtmp_relay.pid") != 0)
                {
                    Log(Log::Level::WARN) << "Failed to delete pid file";
                }

                if (kill(pid, SIGTERM) != 0)
                {
                    Log(Log::Level::ERR) << "Failed to kill daemon";
                    return EXIT_FAILURE;
                }

                Log(Log::Level::INFO) << "Daemon killed";

                return EXIT_SUCCESS;
            }
            else
            {
                Log(Log::Level::ERR) << "Failed to get the pid of the daemon";
                return EXIT_FAILURE;
            }
#else
            Log(Log::Level::ERR) << "Daemon is not supported on Windows";
            return EXIT_FAILURE;
#endif
        }
        else if (std::string(argv[i]) == "--help")
        {
            const char* exe = argc >= 1 ? argv[0] : "rtmp_relay";
            Log(Log::Level::INFO) << "Usage: " << exe << " --config <path to config file> [--daemon] [--kill-daemon] [--log <level>]";
            return EXIT_SUCCESS;
        }
        else if (std::string(argv[i]) == "--version")
        {
            Log(Log::Level::INFO) << "RTMP relay v" << static_cast<uint32_t>(RTMP_RELAY_VERSION[0]) << "." << static_cast<uint32_t>(RTMP_RELAY_VERSION[1]);
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
#ifndef _WIN32
        if (!daemonize("/var/run/rtmp_relay.pid")) return EXIT_FAILURE;
#else
        Log(Log::Level::ERR) << "Daemon is not supported on Windows";
        return EXIT_FAILURE;
#endif
    }

#ifndef _WIN32
    if (std::signal(SIGUSR1, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGUSR1";
        return EXIT_FAILURE;
    }

    if (std::signal(SIGPIPE, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGPIPE";
        return EXIT_FAILURE;
    }

    // software termination signal from kill
    if (std::signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        Log(Log::Level::ERR) << "Failed to capure SIGTERM";
        return false;
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
