//
//  rtmp_relay
//

#include "Connection.h"

namespace relay
{
    Connection::Connection(cppsocket::Socket& aSocket):
        socket(aSocket)
    {
    }

    void Connection::update()
    {
    }
}
