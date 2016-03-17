//
//  rtmp_relay
//

#pragma once

enum class State
{
    UNINITIALIZED = 0,
    VERSION_RECEIVED = 1,
    VERSION_SENT = 2,
    ACK_SENT = 3,
    HANDSHAKE_DONE = 4
};

enum HeaderType
{
    TWELVE_BYTE_HEADER = 0x00, // 00
    EIGHT_BYTE_HEADER = 0x01, // 01
    FOUR_BYTE_HEADER = 0x02, // 10
    ONE_BYTE_HEADER = 0x03 // 11
};

struct Challange
{
    uint32_t time;
    uint8_t version[4];
    uint8_t randomBytes[1528];
};

struct Ack
{
    uint32_t time;
    uint32_t time2;
    uint8_t randomBytes[1528];
};
