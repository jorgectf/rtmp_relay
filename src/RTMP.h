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

enum class HeaderType
{
    TWELVE_BYTE = 0x00, // 00
    EIGHT_BYTE = 0x01, // 01
    FOUR_BYTE = 0x02, // 10
    ONE_BYTE = 0x03 // 11
};

enum class MessageType
{
    SET_CHUNK_SIZE = 0x01,
    PING = 0x04,
    SERVER_BANDWIDTH = 0x05,
    CLIENT_BANDWIDTH = 0x06,
    AUDIO_PACKET = 0x08,
    VIDEO_PACKET = 0x09,
    AMD3_COMMAND = 0x11,
    INVOKE = 0x12, // e.g. onMetaData
    AMF0_COMMAND = 0x14
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
