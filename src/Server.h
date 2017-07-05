//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Connection.h"
#include "Stream.h"

namespace relay
{
    class Server
    {
    public:
        Server(Relay& aRelay, cppsocket::Network& aNetwork);

        uint64_t getId() const { return id; }

        Stream* findStream(Connection::StreamType type,
                           const std::string& applicationName,
                           const std::string& streamName) const;
        Stream* createStream(Connection::StreamType type,
                             const std::string& applicationName,
                             const std::string& streamName);
        void releaseStream(Stream* stream);

        void start(const std::vector<Connection::Description>& aConnectionDescriptions);

        void update(float delta);
        void getStats(std::string& str, ReportType reportType) const;

        const std::vector<Connection::Description>& getConnectionDescriptions() const { return connectionDescriptions; }

    private:
        Relay& relay;
        const uint64_t id;

        cppsocket::Network& network;
        std::vector<Connection::Description> connectionDescriptions;

        std::vector<std::unique_ptr<Stream>> streams;
        std::vector<std::unique_ptr<Connection>> connections;
    };
}
