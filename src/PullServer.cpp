//
//  rtmp_relay
//

#include "PullServer.h"

namespace relay
{
    PullServer::PullServer(cppsocket::Network& aNetwork,
                           const std::string& aApplication,
                           const std::string& aOverrideStreamName,
                           const std::string& aAddress,
                           bool videoOutput,
                           bool audioOutput,
                           bool dataOutput,
                           const std::set<std::string>& aMetaDataBlacklist,
                           float aPingInterval):
        network(aNetwork),
        socket(aNetwork),
        application(aApplication),
        overrideStreamName(aOverrideStreamName),
        address(aAddress),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput),
        metaDataBlacklist(aMetaDataBlacklist),
        pingInterval(aPingInterval)
    {
        socket.setAcceptCallback(std::bind(&PullServer::handleAccept, this, std::placeholders::_1));

        socket.startAccept(address);
    }

    void PullServer::update(float delta)
    {
        for (auto senderIterator = pullSenders.begin(); senderIterator != pullSenders.end();)
        {
            const auto& pullSender = *senderIterator;

            if (pullSender->isConnected())
            {
                pullSender->update(delta);
                ++senderIterator;
            }
            else
            {
                senderIterator = pullSenders.erase(senderIterator);
            }
        }
    }

    void PullServer::createStream(const std::string& newStreamName)
    {
        streamName = newStreamName;

        for (const auto& sender : pullSenders)
        {
            sender->createStream(streamName);
        }
    }

    void PullServer::deleteStream()
    {
        streamName.clear();

        for (const auto& sender : pullSenders)
        {
            sender->deleteStream();
        }
    }

    void PullServer::unpublishStream()
    {
        for (const auto& sender : pullSenders)
        {
            sender->unpublishStream();
        }
    }

    void PullServer::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (const auto& sender : pullSenders)
        {
            sender->sendAudioHeader(headerData);
        }
    }

    void PullServer::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        videoHeader = headerData;

        for (const auto& sender : pullSenders)
        {
            sender->sendVideoHeader(headerData);
        }
    }

    void PullServer::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendAudio(timestamp, audioData);
        }
    }

    void PullServer::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendVideo(timestamp, videoData);
        }
    }

    void PullServer::sendMetaData(const amf0::Node& newMetaData)
    {
        metaData = newMetaData;

        for (const auto& sender : pullSenders)
        {
            sender->sendMetaData(newMetaData);
        }
    }
    
    void PullServer::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendTextData(timestamp, textData);
        }
    }

    void PullServer::handleAccept(cppsocket::Socket& clientSocket)
    {
        std::auto_ptr<PullSender> pullSender(new PullSender(clientSocket,
                                                            application,
                                                            overrideStreamName,
                                                            videoStream,
                                                            audioStream,
                                                            dataStream,
                                                            metaDataBlacklist,
                                                            pingInterval));

        if (!streamName.empty())
        {
            pullSender->createStream(streamName);
        }

        if (!audioHeader.empty())
        {
            pullSender->sendAudioHeader(audioHeader);
        }

        if (!videoHeader.empty())
        {
            pullSender->sendVideoHeader(videoHeader);
        }

        if (metaData.getMarker() != amf0::Marker::Unknown)
        {
            pullSender->sendMetaData(metaData);
        }

        pullSenders.push_back(std::move(pullSender));
    }

    void PullServer::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "Application: " + application + "\n";
                str += "Pull senders:";
                for (const auto& sender : pullSenders)
                {
                    sender->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str += "Application: " + application;

                str += "<h3>Pull senders</h3><table border=\"1\"><tr><th>Name</th><th>Connected</th><th>Address</th><th>State</th></tr>";

                for (const auto& sender : pullSenders)
                {
                    sender->getInfo(str, reportType);
                }

                str += "</table>";
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"application\":\"" + application + "\"," +
                "\"pullSenders\":[";

                bool first = true;

                for (const auto& sender : pullSenders)
                {
                    if (!first) str += ",";
                    first = false;
                    sender->getInfo(str, reportType);
                }

                str += "]}";
                break;
            }
        }
    }
}
