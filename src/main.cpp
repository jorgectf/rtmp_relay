//
//  rtmp_relay
//

#include <cstdlib>
#include <iostream>
#include <signal.h>

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "Relay.h"

static std::string config;
cppsocket::Network network;
relay::Relay rel(network);

#ifndef _MSC_VER
static void signalHandler(int signo)
{
    switch(signo)
    {
        case SIGHUP:
            // rehash the server
            if (!rel.init(config))
            {
                std::cerr << "Failed to reload config" << std::endl;
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
            std::cerr << "Received SIGPIPE" << std::endl;
            break;
    }
}

static int daemonize(const char* lock_file)
{
    // drop to having init() as parent
    int i, lfp, pid = fork();
    char str[256] = {0};
    if (pid < 0)
    {
        std::cerr << "Failed to fork process" << std::endl;
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

    lfp = open(lock_file, O_RDWR|O_CREAT|O_EXCL, 0640);

    if (lfp < 0)
    {
        std::cerr << "Failed to open lock file" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (lockf(lfp, F_TLOCK, 0) < 0)
    {
        std::cerr << "Failed to lock the file" << std::endl;
        exit(EXIT_SUCCESS);
    }

    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); // record pid to lockfile

    // ignore child terminate signal
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        std::cerr << "Failed to ignore SIGCHLD" << std::endl;
        exit(EXIT_FAILURE);
    }

    // hangup signal
    if (signal(SIGHUP, signalHandler) == SIG_ERR)
    {
        std::cerr << "Failed to capure SIGHUP" << std::endl;
        exit(EXIT_FAILURE);
    }

    // software termination signal from kill
    if (signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        std::cerr << "Failed to capure SIGTERM" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Daemon started, pid: " << getpid() << std::endl;

    return EXIT_SUCCESS;
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
        else if (strcmp(argv[i], "--help") == 0)
        {
            const char* exe = argc >= 1 ? argv[0] : "rtmp_relay";
            std::cout << "Usage: " << exe << " --config <path to config file>" << std::endl;
            return EXIT_SUCCESS;
        }
    }

    if (config.empty())
    {
        std::cerr << "No config file" << std::endl;
        return EXIT_FAILURE;
    }

    if (daemon)
    {
#ifdef _MSC_VER
        std::cerr << "Daemon not supported on Windows" << std::endl;
        return EXIT_FAILURE;
#else
        if (daemonize("/var/run/rtmp_relay.pid") == -1) return EXIT_FAILURE;
#endif
    }

#ifndef _MSC_VER
    if (signal(SIGUSR1, signalHandler) == SIG_ERR)
    {
        std::cerr << "Failed to capure SIGUSR1" << std::endl;
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, signalHandler) == SIG_ERR)
    {
        std::cerr << "Failed to capure SIGPIPE" << std::endl;
        return EXIT_FAILURE;
    }
#endif
    
    if (!rel.init(config))
    {
        std::cerr << "Failed to init relay" << std::endl;
        return EXIT_FAILURE;
    }
    
    rel.run();
    
    return EXIT_SUCCESS;
}
