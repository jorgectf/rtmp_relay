//
//  rtmp_relay
//

#include <iostream>
#include "Application.h"
#include "Log.h"

namespace relay
{
    Application::Application(cppsocket::Network& pNetwork,
                             const ApplicationDescriptor& applicationDescriptor,
                             const std::string& pName):
        name(pName)
    {
        for (const SenderDescriptor& senderDescriptor : applicationDescriptor.senderDescriptors)
        {
            std::unique_ptr<Sender> sender(new Sender(pNetwork,
                                                      name,
                                                      senderDescriptor.addresses,
                                                      senderDescriptor.videoOutput,
                                                      senderDescriptor.audioOutput,
                                                      senderDescriptor.dataOutput,
                                                      senderDescriptor.metaDataBlacklist,
                                                      senderDescriptor.connectionTimeout,
                                                      senderDescriptor.reconnectInterval,
                                                      senderDescriptor.reconnectCount));

            sender->connect();

            senders.push_back(std::move(sender));
        }
    }

    void Application::update(float delta)
    {
        for (const auto& sender : senders)
        {
            sender->update(delta);
        }
    }

    void Application::createStream(const std::string& streamName)
    {
        for (const auto& sender : senders)
        {
            sender->createStream(streamName);
        }
    }

    void Application::deleteStream()
    {
        for (const auto& sender : senders)
        {
            sender->deleteStream();
        }
    }

    void Application::unpublishStream()
    {
        for (const auto& sender : senders)
        {
            sender->unpublishStream();
        }
    }

    void Application::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const auto& sender : senders)
        {
            sender->sendAudio(timestamp, audioData);
        }
    }

    void Application::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const auto& sender : senders)
        {
            sender->sendVideo(timestamp, videoData);
        }
    }

    void Application::sendMetaData(const amf0::Node& metaData)
    {
        for (const auto& sender : senders)
        {
            sender->sendMetaData(metaData);
        }
    }

    void Application::sendTextData(const amf0::Node& textData)
    {
        for (const auto& sender : senders)
        {
            sender->sendTextData(textData);
        }
    }

    void Application::printInfo() const
    {
        Log(Log::Level::INFO) << "Application: " << name;

        Log(Log::Level::INFO) << "Senders:";
        for (const auto& sender : senders)
        {
            sender->printInfo();
        }
    }
}
