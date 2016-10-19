//
//  rtmp_relay
//

#include "Status.h"

namespace relay
{
    Status::Status(cppsocket::Network& aNetwork, Relay& aRelay):
        network(aNetwork), relay(aRelay), socket(aNetwork)
    {
        //socket.setConnectTimeout(connectionTimeout);
        socket.setAcceptCallback(std::bind(&Status::handleAccept, this, std::placeholders::_1));

        socket.startAccept("0.0.0.0:80");
    }

    void Status::handleAccept(cppsocket::Socket& clientSocket)
    {
        clientSocket.setReadCallback(std::bind(&Status::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        clientSocket.setCloseCallback(std::bind(&Status::handleClose, this, std::placeholders::_1));
    }

    void Status::handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData)
    {
    }

    void Status::handleClose(cppsocket::Socket&)
    {
    }
}
