//
//  rtmp_relay
//

#include "Server.hpp"
#include "Relay.hpp"

using namespace cppsocket;

namespace relay
{
    Server::Server(Relay& aRelay,
                   cppsocket::Network& aNetwork):
        relay(aRelay),
        id(Relay::nextId()),
        network(aNetwork)
    {
    }

    Stream* Server::findStream(Stream::Type type,
                               const std::string& applicationName,
                               const std::string& streamName) const
    {
        for (auto i = streams.begin(); i != streams.end(); ++i)
        {
            if ((*i)->getType() == type &&
                (*i)->getApplicationName() == applicationName &&
                (*i)->getStreamName() == streamName)
            {
                return i->get();
            }
        }

        return nullptr;
    }

    Connection* Server::createConnection(Stream& stream,
                                         const Endpoint& endpoint)
    {
        std::unique_ptr<Connection> connection(new Connection(relay, stream, endpoint));
        Connection* connectionPtr = connection.get();
        connections.push_back(std::move(connection));

        return connectionPtr;
    }

    void Server::deleteConnection(Connection* connection)
    {
        for (auto i = connections.begin(); i != connections.end();)
        {
            if (i->get() == connection)
            {
                i = connections.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }

    Stream* Server::createStream(Stream::Type type,
                                 const std::string& applicationName,
                                 const std::string& streamName)
    {
        std::unique_ptr<Stream> stream(new Stream(*this, type, applicationName, streamName));
        Stream* streamPtr = stream.get();
        streams.push_back(std::move(stream));

        return streamPtr;
    }

    void Server::deleteStream(Stream* stream)
    {
        for (auto i = streams.begin(); i != streams.end();)
        {
            if (i->get() == stream)
            {
                i = streams.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }

    void Server::start(const std::vector<Endpoint>& aEndpoints)
    {
        endpoints = aEndpoints;

        for (const Endpoint& endpoint : endpoints)
        {
            if (endpoint.connectionType == Connection::Type::CLIENT &&
                endpoint.streamType == Stream::Type::INPUT)
            {
                Socket socket(network);

                Stream* stream = createStream(endpoint.streamType,
                                              endpoint.applicationName,
                                              endpoint.streamName);

                std::unique_ptr<Connection> connection(new Connection(relay,
                                                                      *stream,
                                                                      endpoint));

                connection->setStream(stream);

                connection->connect();

                connections.push_back(std::move(connection));
            }
        }
    }

    void Server::update(float delta)
    {
        for (auto i = connections.begin(); i != connections.end();)
        {
            const std::unique_ptr<Connection>& connection = *i;

            connection->update(delta);

            if (connection->isClosed())
            {
                i = connections.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }

    void Server::getStats(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                for (const auto& connection : connections)
                {
                    connection->getStats(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                for (const auto& connection : connections)
                {
                    connection->getStats(str, reportType);
                }
                break;
            }
            case ReportType::JSON:
            {
                bool first = true;

                for (const auto& connection : connections)
                {
                    if (!first) str += ",";
                    first = false;
                    connection->getStats(str, reportType);
                }
                break;
            }
        }
    }
}
