//
//  rtmp_relay
//

#pragma once

#include <memory>
#include <random>
#include <vector>
#include <utility>
#include <chrono>
#include "Network.hpp"
#include "Socket.hpp"
#include "Status.hpp"
#include "Server.hpp"
#include "Endpoint.hpp"

#ifndef _WIN32
#include <sys/syslog.h>
#endif

namespace relay
{
    class Status;

    class Relay
    {
    public:
        static uint64_t nextId() { return ++currentId; }

        Relay(cppsocket::Network& aNetwork);
        ~Relay();

        Relay(const Relay&) = delete;
        Relay(Relay&&) = delete;
        Relay& operator=(const Relay&) = delete;
        Relay& operator=(Relay&&) = delete;

        std::mt19937& getGenerator() { return generator; }
        cppsocket::Network& getNetwork() { return network; }

        bool init(const std::string& config);
        void close();

        void run();

        void getStats(std::string& str, ReportType reportType) const;

        void openLog();
        void closeLog();

        std::vector<std::pair<Server*, const Endpoint*>> getEndpoints(const std::pair<uint32_t, uint16_t>& address,
                                                                      Connection::Direction type,
                                                                      const std::string& apyplicationName,
                                                                      const std::string& streamName) const;

    private:
        void handleAccept(cppsocket::Socket& acceptor, cppsocket::Socket& clientSocket);

        static uint64_t currentId;
        std::mt19937 generator;
        bool active = true;

        cppsocket::Network& network;
        std::unique_ptr<Status> status;
        std::chrono::steady_clock::time_point previousTime;
        std::chrono::steady_clock::time_point timeout;
        bool hasTimeout = false;

        std::vector<std::unique_ptr<Server>> servers;
        std::vector<std::unique_ptr<Connection>> connections;

        std::vector<cppsocket::Socket> acceptors;

#ifndef _WIN32
        std::string syslogIdent;
        int syslogFacility = LOG_USER;
#endif
    };
}
