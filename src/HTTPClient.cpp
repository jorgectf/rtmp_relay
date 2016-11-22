//
//  rtmp_relay
//

#include "HTTPClient.h"

namespace relay
{
    HTTPClient::HTTPClient(cppsocket::Network& aNetwork,
                           cppsocket::Socket& aSocket,
                           Relay& aRelay):
        network(aNetwork),
        socket(aSocket),
        relay(aRelay)
    {
    }
}
