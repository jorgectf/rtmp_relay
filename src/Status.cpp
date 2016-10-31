//
//  rtmp_relay
//

#include "Status.h"

namespace relay
{
    Status::Status(cppsocket::Network& aNetwork, Relay& aRelay, const std::string& address):
        network(aNetwork), socket(aNetwork), relay(aRelay)
    {
        //socket.setConnectTimeout(connectionTimeout);
        socket.setAcceptCallback(std::bind(&Status::handleAccept, this, std::placeholders::_1));

        socket.startAccept(address);
    }

    void Status::handleAccept(cppsocket::Socket& clientSocket)
    {
        clientSocket.setReadCallback(std::bind(&Status::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        clientSocket.setCloseCallback(std::bind(&Status::handleClose, this, std::placeholders::_1));

        clients.push_back(std::move(clientSocket));
    }

    void Status::handleRead(cppsocket::Socket& clientSocket, const std::vector<uint8_t>&)
    {
        std::string info;
        relay.getInfo(info);

        std::string response = "HTTP/1.0 200 OK\r\n"
            "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
            "\r\n"
            "<html><title>Status</title><body>" + info + "</body></html>";

        std::vector<uint8_t> data(response.begin(), response.end());

        clientSocket.send(data);

        for (auto i = clients.begin(); i != clients.end(); ++i)
        {
            if (&(*i) == &clientSocket)
            {
                clients.erase(i);
                break;
            }
            else
            {
                ++i;
            }
        }
    }

    void Status::handleClose(cppsocket::Socket& clientSocket)
    {
        for (auto i = clients.begin(); i != clients.end(); ++i)
        {
            if (&(*i) == &clientSocket)
            {
                clients.erase(i);
                break;
            }
            else
            {
                ++i;
            }
        }
    }
}
