//
//  rtmp_relay
//

#include "Status.h"

namespace relay
{
    Status::Status(cppsocket::Network& aNetwork, Relay& aRelay):
        network(aNetwork), relay(aRelay), socket(aNetwork)
    {
        socket.startAccept("0.0.0.0:80");

        //socket.setConnectTimeout(connectionTimeout);
        socket.setAcceptCallback(std::bind(&Status::handleAccept, this, std::placeholders::_1));
        socket.setReadCallback(std::bind(&Status::handleRead, this, std::placeholders::_1));
        socket.setCloseCallback(std::bind(&Status::handleClose, this));
        socket.startRead();
    }

    void Status::handleAccept(cppsocket::Socket& clientSocket)
    {
    }

    void Status::handleRead(const std::vector<uint8_t>& newData)
    {
    }

    void Status::handleClose()
    {
    }
}
