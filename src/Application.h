//
//  rtmp_relay
//

#pragma once

#include <string>
#include <set>
#include <vector>
#include <map>
#include "Network.h"
#include "Connector.h"
#include "RTMP.h"
#include "Server.h"

namespace relay
{
    class Application
    {
    public:
        Application(cppsocket::Network& aNetwork,
                    const ApplicationDescriptor& applicationDescriptor,
                    const std::string& aName);
        virtual ~Application() {}

        const std::string& getName() const { return name; }

        void update(float delta);

        void createStream(const std::string& streamName);
        void deleteStream();
        void unpublishStream();

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& metaData);
        void sendTextData(const amf0::Node& textData);

        void printInfo() const;
        void getInfo(std::string& str) const;

    private:
        std::string name;

        std::vector<std::unique_ptr<Sender>> senders;
    };
}
