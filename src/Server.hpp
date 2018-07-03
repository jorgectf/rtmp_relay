//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Connection.hpp"
#include "Endpoint.hpp"
#include "Stream.hpp"

namespace relay
{
    class Server
    {
    public:
        Server(Relay& aRelay, Network& aNetwork);

        Server(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(const Server&) = delete;
        Server& operator=(Server&&) = delete;

        uint64_t getId() const { return id; }

        Connection* createConnection(Stream& stream,
                                     const Endpoint& endpoint);

        Stream* findStream(const std::string& applicationName,
                           const std::string& streamName) const;
        Stream* createStream(const std::string& applicationName,
                             const std::string& streamName);
        void deleteStream(Stream* stream);

        void start(const std::vector<Endpoint>& aEndpoints);

        void update(float delta);
        void getStats(std::string& str, ReportType reportType) const;

        const std::vector<Endpoint>& getEndpoints() const { return endpoints; }
        void cleanup() { needsCleanup = true; }
        void getConnections(std::map<Connection*, Stream*>& cons);

        void stop();

    private:
        Relay& relay;
        const uint64_t id;

        Network& network;
        std::vector<Endpoint> endpoints;

        std::vector<std::unique_ptr<Stream>> streams;
        std::vector<std::unique_ptr<Connection>> connections;

        bool needsCleanup = false;

        void deleteConnection(Connection* connection);
    };
}
