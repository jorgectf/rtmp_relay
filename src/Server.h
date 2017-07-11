//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Connection.h"
#include "Endpoint.h"
#include "Stream.h"

namespace relay
{
    class Server
    {
    public:
        Server(Relay& aRelay, cppsocket::Network& aNetwork);

        uint64_t getId() const { return id; }

        Stream* findStream(Stream::Type type,
                           const std::string& applicationName,
                           const std::string& streamName) const;
        Stream* createStream(Stream::Type type,
                             const std::string& applicationName,
                             const std::string& streamName);
        void releaseStream(Stream* stream);

        void start(const std::vector<Endpoint>& aEndpoints);

        void update(float delta);
        void getStats(std::string& str, ReportType reportType) const;

        const std::vector<Endpoint>& getEndpoints() const { return endpoints; }

    private:
        Relay& relay;
        const uint64_t id;

        cppsocket::Network& network;
        std::vector<Endpoint> endpoints;

        std::vector<std::unique_ptr<Stream>> streams;
        std::vector<std::unique_ptr<Connection>> connections;
    };
}
