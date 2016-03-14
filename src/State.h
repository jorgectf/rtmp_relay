//
//  rtmp_relay
//

#pragma once

enum class State
{
    UNINITIALIZED = 0,
    VERSION_SENT = 1,
    ACK_SENT = 2,
    HANDSHAKE_DONE = 3
};
