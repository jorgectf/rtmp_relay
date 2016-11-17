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
        // accept only one input
        if (receivers.empty())
        {
            std::unique_ptr<Receiver> receiver(new Receiver(network, clientSocket, pingInterval, applicationDescriptors));
            receivers.push_back(std::move(receiver));
        }
        else
        {
            clientSocket.close();
        }
    }

    void Server::printInfo() const
    {
        Log(Log::Level::INFO) << "Server listening on " << socket.getPort();

        Log(Log::Level::INFO) << "Receivers:";
        for (const auto& receiver : receivers)
        {
            receiver->printInfo();
        }
    }

    void Server::getInfo(std::string& str) const
    {
        str += "<h1>Receivers</h1><table><tr><th>Name</th><th>Connected</th><th>Address</th><th>State</th></tr>";
        for (const auto& receiver : receivers)
        {
            receiver->getInfo(str);
        }

        str += "</table>";
    }
}
