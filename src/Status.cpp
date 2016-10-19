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
    }
}
