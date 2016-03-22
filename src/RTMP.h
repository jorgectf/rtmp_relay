//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <vector>

namespace rtmp
{
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
        UNKNOWN = 0x00,
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
    
    struct Header
    {
        HeaderType type;
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
