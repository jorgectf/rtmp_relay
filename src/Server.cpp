//
//  rtmp_relay
//

#include <queue>
#include <iostream>
#include <algorithm>
#include "Server.h"
#include "Relay.h"
#include "Application.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Server::Server(Network& aNetwork,
                   const std::string& address,
                   float aPingInterval,
                   const std::vector<ApplicationDescriptor>& aApplicationDescriptors):
        id(Relay::nextId()),
        network(aNetwork),
        socket(aNetwork),
        pingInterval(aPingInterval),
        applicationDescriptors(aApplicationDescriptors)
    {
        socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));

        socket.startAccept(address);
    }

    void Server::update(float delta)
    {
        for (auto receiverIterator = receivers.begin(); receiverIterator != receivers.end();)
        {
            const auto& receiver = *receiverIterator;

            if (receiver->isConnected())
            {
                receiver->update(delta);
                ++receiverIterator;
            }
            else
            {
                receiverIterator = receivers.erase(receiverIterator);
            }
        }
    }

    void Server::handleAccept(Socket& clientSocket)
    {
        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input connected";

        std::unique_ptr<PushReceiver> receiver(new PushReceiver(network, clientSocket, pingInterval, applicationDescriptors));
        receivers.push_back(std::move(receiver));
    }

    void Server::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "Server " + std::to_string(id) + " listening on " + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + "\n";

                str += "Receivers:\n";

                for (const auto& receiver : receivers)
                {
                    receiver->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str += "<h1>Server " + std::to_string(id) + "</h1>";
                str += "Address: " + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort());

                for (const auto& receiver : receivers)
                {
                    receiver->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"id\":" + std::to_string(id) + ",\"address\":\"" + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + "\",\"receivers\":[";

                bool first = true;

                for (const auto& receiver : receivers)
                {
                    if (!first) str += ",";
                    first = false;
                    receiver->getInfo(str, reportType);
                }
                str += "]}";
                break;
            }
        }
    }
}
