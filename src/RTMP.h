//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <vector>
#include <map>

namespace rtmp
{
    const uint32_t CLIENT_STREAM_ID_AMF_INI = 3;
    const uint32_t CLIENT_STREAM_ID_AMF = 5;
    const uint32_t CLIENT_STREAM_ID_AUDIO = 6;
    const uint32_t CLIENT_STREAM_ID_VIDEO = 7;

    enum class Channel: uint32_t
    {
        NONE = 0,

        NETWORK = 2,   // channel for network-related messages (bandwidth report, ping, etc)
        SYSTEM = 3,    // channel for sending server control messages
        AUDIO = 4,     // channel for audio data
        VIDEO   = 6,   // channel for video data
        SOURCE  = 8,   // channel for a/v invokes
    };

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
        NONE = 0,

        SET_CHUNK_SIZE = 1,     // chunk size change
        BYTES_READ = 3,         // number of bytes read
        PING = 4,               // ping
        SERVER_BANDWIDTH = 5,   // server bandwidth
        CLIENT_BANDWIDTH = 6,   // client bandwidth
        AUDIO_PACKET = 8,       // audio packet
        VIDEO_PACKET = 9,       // video packet
        FLEX_STREAM = 15,       // Flex shared stream
        FLEX_OBJECT = 16,       // Flex shared object
        FLEX_MESSAGE = 17,      // Flex shared message
        NOTIFY = 18,            // some notification
        SHARED_OBJ = 19,        // shared object
        INVOKE = 20,            // invoke some stream action
        METADATA = 22           // FLV metadata
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
            NONE = 0xff,

            TWELVE_BYTE = 0x00, // bits 00, 12-byte header
            EIGHT_BYTE = 0x01,  // bits 01, 8-byte header
            FOUR_BYTE = 0x02,   // bits 10, 4-byte header
            ONE_BYTE = 0x03     // bits 11, 1-byte header

        };

        Type type = Type::NONE;
        Channel channel = Channel::NONE;
        uint32_t ts = 0; // 3-btye timestamp field
        uint32_t length = 0;
        MessageType messageType = MessageType::NONE;
        uint32_t messageStreamId = 0;
        uint32_t timestamp = 0; // final timestamp (either from 3-byte timestamp or extended timestamp fields)
    };
    
    struct Packet
    {
        Header header;
        std::vector<uint8_t> data;
    };
    
    struct Challenge
    {
        uint32_t time;
        uint8_t version[4];
        uint8_t randomBytes[1528];
    };

    struct Ack
    {
        uint32_t time;
        uint8_t version[4];
        uint8_t randomBytes[1528];
    };

    uint32_t decodePacket(const std::vector<uint8_t>& data, uint32_t offset, uint32_t chunkSize, Packet& packet, std::map<rtmp::Channel, rtmp::Header>& previousPackets);
    uint32_t encodePacket(std::vector<uint8_t>& data, uint32_t chunkSize, const Packet& packet);
}
