//
//  rtmp_relay
//

#include <algorithm>
#include "StatusClient.h"
#include "Relay.h"
#include "Utils.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    StatusClient::StatusClient(cppsocket::Network& aNetwork,
                               cppsocket::Socket& aSocket,
                               Relay& aRelay):
        network(aNetwork),
        socket(std::move(aSocket)),
        relay(aRelay)
    {
        socket.setReadCallback(std::bind(&StatusClient::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        socket.setCloseCallback(std::bind(&StatusClient::handleClose, this, std::placeholders::_1));
    }

    void StatusClient::handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData)
    {
        const std::vector<uint8_t> clrf = { '\r', '\n' };

        data.insert(data.end(), newData.begin(), newData.end());

        for (;;)
        {
            auto i = std::search(data.begin(), data.end(), clrf.begin(), clrf.end());

            if (i == data.end())
            {
                break;
            }

            std::string line(data.begin(), i);

            if (line.empty()) // end of header
            {
                if (!startLine.empty()) // received header
                {
                    sendReport();
                    socket.close();
                    break;
                }
            }
            else
            {
                if (startLine.empty())
                {
                    startLine = line;
                }
                else
                {
                    headers.push_back(line);
                }
            }

            data.erase(data.begin(), i + 2);
        }
    }

    void StatusClient::handleClose(cppsocket::Socket&)
    {
    }

    void StatusClient::sendReport()
    {
        std::vector<std::string> fields;
        tokenize(startLine, fields);

        if (fields.size() >= 2 && fields[0] == "GET")
        {
            if (fields[1] == "/stats")
            {
                std::string info;
                relay.getInfo(info, ReportType::HTML);

                std::string response = "HTTP/1.1 200 OK\r\n"
                    "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: " + std::to_string(info.length()) + "\r\n"
                    "\r\n" + info;


                std::vector<uint8_t> buffer(response.begin(), response.end());

                socket.send(buffer);
            }
            else if (fields[1] == "/stats.txt")
            {
                std::string info;
                relay.getInfo(info, ReportType::TEXT);

                std::string response = "HTTP/1.1 200 OK\r\n"
                "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(info.length()) + "\r\n"
                "\r\n" + info;

                std::vector<uint8_t> buffer(response.begin(), response.end());

                socket.send(buffer);
            }
            else if (fields[1] == "/stats.json")
            {
                std::string info;
                relay.getInfo(info, ReportType::JSON);

                std::string response = "HTTP/1.1 200 OK\r\n"
                "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + std::to_string(info.length()) + "\r\n"
                "\r\n" + info;

                std::vector<uint8_t> buffer(response.begin(), response.end());

                socket.send(buffer);
            }
            else
            {
                sendError();
            }
        }
        else
        {
            sendError();
        }
    }

    void StatusClient::sendError()
    {
        std::string response = "HTTP/1.1 404 Not Found\r\n"
        "Last-modified: Fri, 09 Aug 1996 14:21:40 GMT\r\n"
        "\r\n";

        std::vector<uint8_t> buffer(response.begin(), response.end());

        socket.send(buffer);
    }
}
