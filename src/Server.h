//
//  rtmp_relay
//

#pragma once

#include "Connection.h"

namespace relay
{
    class Server
    {
    public:
        struct InputDescription
        {
            Connection::Description connectionDescription;
            bool video = true;
            bool audio = true;
            bool data = true;
            std::string applicationName;
            std::string streamName;
        };

        struct OutputDescription
        {
            Connection::Description connectionDescription;
            bool video = true;
            bool audio = true;
            bool data = true;
            std::string overrideApplicationName;
            std::string overrideStreamName;
        };

        struct Description
        {
            std::vector<InputDescription> inputDescriptions;
            std::vector<OutputDescription> outputDescriptions;
        };

        Server(const Server::Description& aDescription);

        const Server::Description& getDescription() const { return description; }

    private:
        Server::Description description;
    };
}
