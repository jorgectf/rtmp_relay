//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <vector>

namespace rtmp
{
    const uint32_t MESSAGE_STREAM_ID = 1;

    const uint32_t CLIENT_STREAM_ID_AMF_INI = 3;
    const uint32_t CLIENT_STREAM_ID_AMF = 5;
    const uint32_t CLIENT_STREAM_ID_AUDIO = 6;
    const uint32_t CLIENT_STREAM_ID_VIDEO = 7;

    enum class State
    {
        UNINITIALIZED = 0,
        VERSION_RECEIVED = 1,
        VERSION_SENT = 2,
        ACK_SENT = 3,
        HANDSHAKE_DONE = 4
    };

    enum class MessageType: uint8_t
    {
        UNKNOWN = 0x00,
        SET_CHUNK_SIZE = 0x01,
        PING = 0x04,
        SERVER_BANDWIDTH = 0x05,
        CLIENT_BANDWIDTH = 0x06,
        AUDIO_PACKET = 0x08,
        VIDEO_PACKET = 0x09,
        AMF3_COMMAND = 0x11,
        INVOKE = 0x12, // e.g. onMetaData
        AMF0_COMMAND = 0x14
    };

    enum class PingType: uint16_t
    {
        CLEAR_STREAM = 0,
        CLEAR_BUFFER = 1,
        CLIENT_BUFFER_TIME = 3,
        RESET_STREAM = 4,
        PING = 5, // from server to client
        PONG = 6 // from client
    };
    
    struct Header
    {
        enum class Type: uint8_t
        {
            TWELVE_BYTE = 0x00, // 00
            EIGHT_BYTE = 0x01, // 01
            FOUR_BYTE = 0x02, // 10
            ONE_BYTE = 0x03 // 11
        };

        Type type;
        uint8_t chunkStreamId;
        uint32_t timestamp = 0;
        uint32_t length = 0;
        MessageType messageType = MessageType::UNKNOWN;
        uint32_t messageStreamId = 0;
    };
    
    struct Packet
    {
        Header header;
        std::vector<uint8_t> data;
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
    
    uint32_t decodeHeader(const std::vector<uint8_t>& data, uint32_t offset, Header& header);
    uint32_t decodePacket(const std::vector<uint8_t>& data, uint32_t offset, uint32_t chunkSize, Packet& packet);
    
    uint32_t encodeHeader(std::vector<uint8_t>& data, const Header& header);
    uint32_t encodePacket(std::vector<uint8_t>& data, uint32_t chunkSize, const Packet& packet);
}
