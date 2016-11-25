//
//  rtmp_relay
//

#include "Status.h"
#include "Relay.h"
#include "HTTPClient.h"

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
        for (auto i = clients.begin(); i != clients.end();)
        {
            if ((*i)->isConnected())
            {
                ++i;
            }
            else
            {
                i = clients.erase(i);
            }
        }
    }

    void Status::handleAccept(cppsocket::Socket& clientSocket)
    {
        std::unique_ptr<HTTPClient> client(new HTTPClient(network, clientSocket, relay));
        
        clients.push_back(std::move(client));
    }
}
