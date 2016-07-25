//
//  rtmp_relay
//

#include <queue>
#include <iostream>
#include <algorithm>
#include "Server.h"

namespace relay
{
    Server::Server(cppsocket::Network& pNetwork, const std::string& pApplication):
        network(pNetwork), socket(pNetwork), application(pApplication)
    {
        socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
    }

    Server::~Server()
    {
        
    }

    bool Server::init(uint16_t port, const rapidjson::Value& pushArray)
    {
        socket.startAccept(port);

        for (uint32_t pushIndex = 0; pushIndex < static_cast<uint32_t>(pushArray.Size()); ++pushIndex)
        {
            SenderDescriptor senderDescriptor;

            const rapidjson::Value& pushObject = pushArray[pushIndex];

            if (pushObject.HasMember("address"))
            {
                const rapidjson::Value& addressArray = pushObject["address"];

                for (rapidjson::SizeType index = 0; index < addressArray.Size(); ++index)
                {
                    senderDescriptor.addresses.push_back(addressArray[index].GetString());
                }
            }
            else
            {
                senderDescriptor.addresses.push_back("127.0.0.1:1935");
            }

            senderDescriptor.videoOutput = pushObject.HasMember("video") ? pushObject["video"].GetBool() : true;
            senderDescriptor.audioOutput = pushObject.HasMember("audio") ? pushObject["audio"].GetBool() : true;
            senderDescriptor.dataOutput = pushObject.HasMember("data") ? pushObject["data"].GetBool() : true;

            if (pushObject.HasMember("metaDataBlacklist"))
            {
                const rapidjson::Value& metaDataBlacklistArray = pushObject["metaDataBlacklist"];

                for (rapidjson::SizeType index = 0; index < metaDataBlacklistArray.Size(); ++index)
                {
                    const rapidjson::Value& str = metaDataBlacklistArray[index];
                    senderDescriptor.metaDataBlacklist.insert(str.GetString());
                }
            }

            senderDescriptor.connectionTimeout = pushObject.HasMember("connectionTimeout") ? pushObject["connectionTimeout"].GetFloat() : 5.0f;
            senderDescriptor.reconnectInterval = pushObject.HasMember("reconnectInterval") ? pushObject["reconnectInterval"].GetFloat() : 5.0f;
            senderDescriptor.reconnectCount = pushObject.HasMember("reconnectCount") ? pushObject["reconnectCount"].GetUint() : 3;

            senderDescriptors.push_back(senderDescriptor);
        }

        return true;
    }

    void Server::update(float delta)
    {
        for (const auto& sender : senders)
        {
            sender->update(delta);
        }
        
        for (auto receiverIterator = receivers.begin(); receiverIterator != receivers.end();)
        {
            const auto& receiver = *receiverIterator;

            if (receiver->isConnected())
            {
                receiver->update();
                ++receiverIterator;
            }
            else
            {
                close();
                receiverIterator = receivers.erase(receiverIterator);
            }
        }
    }

    void Server::handleAccept(cppsocket::Socket& clientSocket)
    {
        // accept only one input
        if (receivers.empty())
        {
            std::unique_ptr<Receiver> receiver(new Receiver(clientSocket, application, shared_from_this()));
            receivers.push_back(std::move(receiver));

            for (const SenderDescriptor& senderDescriptor : senderDescriptors)
            {
                std::unique_ptr<Sender> sender(new Sender(network,
                                                          application,
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

            open();
        }
        else
        {
            clientSocket.close();
        }
    }

    void Server::open()
    {
        close();
        
        for (const auto& sender : senders)
        {
            sender->connect();
        }
    }

    void Server::close()
    {
        for (const auto& sender : senders)
        {
            sender->disconnect();
        }

        senders.clear();
    }

    void Server::createStream(const std::string& streamName)
    {
        for (const auto& sender : senders)
        {
            sender->createStream(streamName);
        }
    }

    void Server::deleteStream()
    {
        for (const auto& sender : senders)
        {
            sender->deleteStream();
        }
    }

    void Server::unpublishStream()
    {
        for (const auto& sender : senders)
        {
            sender->unpublishStream();
        }
    }

    void Server::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const auto& sender : senders)
        {
            sender->sendAudio(timestamp, audioData);
        }
    }

    void Server::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const auto& sender : senders)
        {
            sender->sendVideo(timestamp, videoData);
        }
    }

    void Server::sendMetaData(const amf0::Node& metaData)
    {
        for (const auto& sender : senders)
        {
            sender->sendMetaData(metaData);
        }
    }

    void Server::sendTextData(const amf0::Node& textData)
    {
        for (const auto& sender : senders)
        {
            sender->sendTextData(textData);
        }
    }

    void Server::printInfo() const
    {
        std::cout << "Server listening on " << socket.getPort() << ", application: " << application << std::endl;

        std::cout << "Senders:" << std::endl;
        for (const auto& sender : senders)
        {
            sender->printInfo();
        }

        std::cout << "Receivers:" << std::endl;
        for (const auto& receiver : receivers)
        {
            receiver->printInfo();
        }
    }
}
