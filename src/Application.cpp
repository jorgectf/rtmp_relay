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
            std::unique_ptr<PushSender> sender(new PushSender(aNetwork,
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

        for (const PullDescriptor& pullDescriptor : applicationDescriptor.pullDescriptors)
        {
            std::unique_ptr<PullServer> server(new PullServer(aNetwork,
                                                              name,
                                                              pullDescriptor.overrideStreamName,
                                                              pullDescriptor.address,
                                                              pullDescriptor.videoOutput,
                                                              pullDescriptor.audioOutput,
                                                              pullDescriptor.dataOutput,
                                                              pullDescriptor.metaDataBlacklist,
                                                              pullDescriptor.pingInterval));

            pullServers.push_back(std::move(server));
        }
    }

    void Application::update(float delta)
    {
        for (const auto& sender : pushSenders)
        {
            sender->update(delta);
        }

        for (const auto& server : pullServers)
        {
            server->update(delta);
        }
    }

    void Application::createStream(const std::string& newStreamName)
    {
        for (const auto& sender : pushSenders)
        {
            sender->createStream(newStreamName);
        }

        for (const auto& server : pullServers)
        {
            server->createStream(newStreamName);
        }
    }

    void Application::deleteStream()
    {
        for (const auto& sender : pushSenders)
        {
            sender->deleteStream();
        }

        for (const auto& server : pullServers)
        {
            server->deleteStream();
        }
    }

    void Application::unpublishStream()
    {
        for (const auto& sender : pushSenders)
        {
            sender->unpublishStream();
        }

        for (const auto& server : pullServers)
        {
            server->unpublishStream();
        }
    }

    void Application::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendAudioHeader(headerData);
        }

        for (const auto& server : pullServers)
        {
            server->sendAudioHeader(headerData);
        }
    }

    void Application::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendVideoHeader(headerData);
        }

        for (const auto& server : pullServers)
        {
            server->sendVideoHeader(headerData);
        }
    }

    void Application::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendAudio(timestamp, audioData);
        }

        for (const auto& server : pullServers)
        {
            server->sendAudio(timestamp, audioData);
        }
    }

    void Application::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendVideo(timestamp, videoData);
        }

        for (const auto& server : pullServers)
        {
            server->sendVideo(timestamp, videoData);
        }
    }

    void Application::sendMetaData(const amf0::Node& metaData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendMetaData(metaData);
        }

        for (const auto& server : pullServers)
        {
            server->sendMetaData(metaData);
        }
    }

    void Application::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (const auto& sender : pushSenders)
        {
            sender->sendTextData(timestamp, textData);
        }

        for (const auto& server : pullServers)
        {
            server->sendTextData(timestamp, textData);
        }
    }

    void Application::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "Application: " + name + "\n";

                str += "Push senders:";
                for (const auto& sender : pushSenders)
                {
                    sender->getInfo(str, reportType);
                }

                str += "\nPull servers:";
                for (const auto& server : pullServers)
                {
                    server->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str += "Application: " + name;

                str += "<h2>Push senders</h2><table border=\"1\"><tr><th>Name</th><th>Connected</th><th>Address</th><th>State</th></tr>";

                for (const auto& sender : pushSenders)
                {
                    sender->getInfo(str, reportType);
                }

                str += "</table>";

                str += "<h2>Pull servers</h2>";

                for (const auto& server : pullServers)
                {
                    server->getInfo(str, reportType);
                }

                break;
            }
            case ReportType::JSON:
            {
                str += "{\"name\":\"" + name + "\"," +
                    "\"pushSenders\":[";

                bool first = true;

                for (const auto& sender : pushSenders)
                {
                    if (!first) str += ",";
                    first = false;
                    sender->getInfo(str, reportType);
                }
                str += "],\"pullServers\":[";

                first = true;

                for (const auto& server : pullServers)
                {
                    if (!first) str += ",";
                    first = false;
                    server->getInfo(str, reportType);
                }

                str += "]}";
                break;
            }
        }
    }
}
