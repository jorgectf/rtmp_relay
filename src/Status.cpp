//
//  rtmp_relay
//

#include "Status.h"
#include "Relay.h"
#include "StatusSender.h"

namespace relay
{
    Status::Status(cppsocket::Network& aNetwork, Relay& aRelay, const std::string& address):
        network(aNetwork), socket(aNetwork), relay(aRelay)
    {
        //socket.setConnectTimeout(connectionTimeout);
        socket.setAcceptCallback(std::bind(&Status::handleAccept, this, std::placeholders::_1));

        socket.startAccept(address);
    }

    void Status::update(float)
    {
        for (auto i = statusSenders.begin(); i != statusSenders.end();)
        {
            if ((*i)->isConnected())
            {
                ++i;
            }
            else
            {
                i = statusSenders.erase(i);
            }
        }
    }

    void Status::handleAccept(cppsocket::Socket& clientSocket)
    {
        std::unique_ptr<StatusSender> statusSender(new StatusSender(network, clientSocket, relay));
        
        statusSenders.push_back(std::move(statusSender));
    }
}
