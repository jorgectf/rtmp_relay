//
//  rtmp_relay
//

#include "Status.h"

namespace relay
{
    Status::Status(cppsocket::Network& aNetwork, Relay& aRelay):
        network(aNetwork), socket(aNetwork), relay(aRelay)
    {
        //socket.setConnectTimeout(connectionTimeout);
        socket.setAcceptCallback(std::bind(&Status::handleAccept, this, std::placeholders::_1));

        socket.startAccept("0.0.0.0:8080");
    }

    void Status::handleAccept(cppsocket::Socket& clientSocket)
    {
        clientSocket.setReadCallback(std::bind(&Status::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        clientSocket.setCloseCallback(std::bind(&Status::handleClose, this, std::placeholders::_1));

        clients.push_back(std::move(clientSocket));
    }

    void Status::handleRead(cppsocket::Socket& clientSocket, const std::vector<uint8_t>&)
    {
        const char response[] = "HTTP/1.0 200 OK\r\n"
            "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
            "\r\n"
            "<TITLE>Hello!</TITLE>";

        std::vector<uint8_t> data(response, response + sizeof(response));

        clientSocket.send(data);
    }

    void Status::handleClose(cppsocket::Socket&)
    {
    }
}
