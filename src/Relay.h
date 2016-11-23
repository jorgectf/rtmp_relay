//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <memory>
#include "Network.h"
#include "Socket.h"
#include "Status.h"

#if !defined(_MSC_VER)
#include <sys/syslog.h>
#endif

namespace relay
{
    class Server;
    class Status;

    class Relay
    {
    public:
        Relay(cppsocket::Network& aNetwork);

        Relay(const Relay&) = delete;
        Relay& operator=(const Relay&) = delete;

        Relay(Relay&&);
        Relay& operator=(Relay&&);

        bool init(const std::string& config);

        void run();

        void getInfo(std::string& str, ReportType reportType) const;

        void openLog();

    private:
        cppsocket::Network& network;
        std::vector<std::unique_ptr<Server>> servers;
        std::unique_ptr<Status> status;
        std::chrono::steady_clock::time_point previousTime;

#if !defined(_MSC_VER)
        std::string syslogIdent;
        int syslogFacility = LOG_USER;
#endif
    };
}
