//
//  rtmp_relay
//

#include <iostream>
#include "Application.h"
#include "Server.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Application::Application(cppsocket::Network& aNetwork,
                             const ApplicationDescriptor& applicationDescriptor,
                             const std::string& aName):
        name(aName)
    {
        for (const PushDescriptor& pushDescriptor : applicationDescriptor.pushDescriptors)
        {
            std::unique_ptr<Push> sender(new Push(aNetwork,
                                                  name,
                                                  pushDescriptor.overrideStreamName,
                                                  pushDescriptor.addresses,
                                                  pushDescriptor.videoOutput,
                                                  pushDescriptor.audioOutput,
                                                  pushDescriptor.dataOutput,
                                                  pushDescriptor.metaDataBlacklist,
                                                  pushDescriptor.connectionTimeout,
                                                  pushDescriptor.reconnectInterval,
                                                  pushDescriptor.reconnectCount));

            sender->connect();

            pushSenders.push_back(std::move(sender));
        }
    }

    void Application::update(float delta)
    {
        for (const auto& sender : pushSenders)
        {
            sender->update(delta);
        }
    }

    void Application::createStream(const std::string& streamName)
    {
        for (const auto& sender : pushSenders)
        {
            sender->createStream(streamName);
        }
    }

    void Application::deleteStream()
    {
        for (const auto& sender : pushSenders)
        {
            sender->deleteStream();
        }
    }

    void Application::unpublishStream()
    {
        for (const auto& sender : pushSenders)
        {
            sender->unpublishStream();
        }
    }

    void Application::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendAudioHeader(headerData);
        }
    }

    void Application::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendVideoHeader(headerData);
        }
    }

    void Application::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendAudio(timestamp, audioData);
        }
    }

    void Application::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendVideo(timestamp, videoData);
        }
    }

    void Application::sendMetaData(const amf0::Node& metaData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendMetaData(metaData);
        }
    }

    void Application::sendTextData(const amf0::Node& textData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendTextData(textData);
        }
    }

    void Application::printInfo() const
    {
        Log(Log::Level::INFO) << "Application: " << name;

        Log(Log::Level::INFO) << "Push senders:";
        for (const auto& sender : pushSenders)
        {
            sender->printInfo();
        }
    }

    void Application::getInfo(std::string& str) const
    {
        str += "Application: " + name;

        str += "<h2>Push senders</h2><table><tr><th>Name</th><th>Connected</th><th>Address</th><th>State</th></tr>";

        for (const auto& sender : pushSenders)
        {
            sender->getInfo(str);
        }

        str += "</table>";
    }
}
