//
//  rtmp_relay
//

#include "Server.h"
#include "Relay.h"

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

    Stream* Server::createStream(Stream::Type type,
                                 const std::string& applicationName,
                                 const std::string& streamName)
    {
        std::unique_ptr<Stream> stream(new Stream(relay, network, *this, type, applicationName, streamName));
        Stream* streamPtr = stream.get();
        streams.push_back(std::move(stream));

        return streamPtr;
    }

    void Server::releaseStream(Stream* stream)
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

    void Server::start(const std::vector<Connection::Description>& aConnectionDescriptions)
    {
        connectionDescriptions = aConnectionDescriptions;

        for (const Connection::Description& connectionDescription : connectionDescriptions)
        {
            if (connectionDescription.type == Connection::Type::CLIENT &&
                connectionDescription.streamType == Stream::Type::INPUT)
            {
                Socket socket(network);

                createStream(connectionDescription.streamType,
                             connectionDescription.applicationName,
                             connectionDescription.streamName);

                std::unique_ptr<Connection> newConnection(new Connection(relay,
                                                                         socket,
                                                                         connectionDescription));

                newConnection->createStream(connectionDescription.applicationName,
                                            connectionDescription.streamName);

                newConnection->connect();

                connections.push_back(std::move(newConnection));
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
