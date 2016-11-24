//
//  rtmp_relay
//

#include "HTTPClient.h"
#include "Relay.h"

namespace relay
{
    HTTPClient::HTTPClient(cppsocket::Network& aNetwork,
                           cppsocket::Socket& aSocket,
                           Relay& aRelay):
        network(aNetwork),
        socket(std::move(aSocket)),
        relay(aRelay)
    {
        socket.setReadCallback(std::bind(&HTTPClient::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        socket.setCloseCallback(std::bind(&HTTPClient::handleClose, this, std::placeholders::_1));
    }

    void HTTPClient::handleRead(cppsocket::Socket& clientSocket, const std::vector<uint8_t>&)
    {
        std::string info;
        relay.getInfo(info, ReportType::HTML);

        std::string response = "HTTP/1.0 200 OK\r\n"
        "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
        "\r\n"
        "<html><title>Status</title><body>" + info + "</body></html>";

        std::vector<uint8_t> data(response.begin(), response.end());

        clientSocket.send(data);
    }

    void HTTPClient::handleClose(cppsocket::Socket&)
    {
    }
}
