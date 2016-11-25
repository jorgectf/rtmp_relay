//
//  rtmp_relay
//

#include <queue>
#include <iostream>
#include <algorithm>
#include "Server.h"
#include "Application.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Server::Server(Network& aNetwork,
                   const std::string& address,
                   float aPingInterval,
                   const std::vector<ApplicationDescriptor>& aApplicationDescriptors):
        network(aNetwork), socket(aNetwork), pingInterval(aPingInterval), applicationDescriptors(aApplicationDescriptors)
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
        std::unique_ptr<Receiver> receiver(new Receiver(network, clientSocket, pingInterval, applicationDescriptors));
        receivers.push_back(std::move(receiver));
    }

    void Server::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "Server listening on " + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + "\n";

                str += "Receivers:\n";

                for (const auto& receiver : receivers)
                {
                    receiver->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str += "<h1>Receivers</h1>";
                for (const auto& receiver : receivers)
                {
                    receiver->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"receivers\": [";
                for (const auto& receiver : receivers)
                {
                    receiver->getInfo(str, reportType);
                }
                str += "]}";
                break;
            }
        }
    }
}
