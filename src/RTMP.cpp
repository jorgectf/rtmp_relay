//
//  rtmp_relay
//

#include <iostream>
#include <algorithm>
#include <cmath>
#include "RTMP.h"
#include "Utils.h"

namespace rtmp
{
    static uint32_t decodeHeader(const std::vector<uint8_t>& data, uint32_t offset, Header& header, std::map<uint32_t, rtmp::Header>& previousPackets)
    {
        uint32_t originalOffset = offset;
        
        if (data.size() - offset < 1)
        {
            return 0;
        }
        
        uint8_t headerData = *(data.data() + offset);
        offset += 1;

        header.channel = static_cast<uint32_t>(headerData & 0x3F);
        header.type = static_cast<Header::Type>(headerData >> 6);

        if (header.channel < 2)
        {
            uint32_t newChannel;
            uint32_t ret = decodeInt(data, offset, header.channel + 1, newChannel);

            if (!ret)
            {
                return false;
            }

            offset += ret;

            header.channel = 64 + newChannel;
        }

#ifdef DEBUG
        std::cout << "Header type: ";

        switch (header.type)
        {
            case Header::Type::TWELVE_BYTE: std::cout << "TWELVE_BYTE"; break;
            case Header::Type::EIGHT_BYTE: std::cout << "EIGHT_BYTE"; break;
            case Header::Type::FOUR_BYTE: std::cout << "FOUR_BYTE"; break;
            case Header::Type::ONE_BYTE: std::cout << "ONE_BYTE"; break;
            default: std::cout << "invalid header type"; break;
        };

        std::cout << "(" << static_cast<uint32_t>(header.type) << ")";

        std::cout << ", channel: " << static_cast<uint32_t>(header.channel);
#endif

        header.length  = previousPackets[header.channel].length;
        header.messageType  = previousPackets[header.channel].messageType;
        header.messageStreamId = previousPackets[header.channel].messageStreamId;
        header.ts = previousPackets[header.channel].ts;
        
        if (header.type != Header::Type::ONE_BYTE)
        {
            uint32_t ret = decodeInt(data, offset, 3, header.ts);

#ifdef DEBUG
            std::cout << ", ts: " << header.ts;

            if (header.ts == 0xFFFFFF)
            {
                std::cout << " (extended)";
            }
#endif
            
            if (!ret)
            {
                return 0;
            }
            
            offset += ret;
            
            if (header.type != Header::Type::FOUR_BYTE)
            {
                ret = decodeInt(data, offset, 3, header.length);

#ifdef DEBUG
                std::cout << ", data length: " << header.length;
#endif
                
                if (!ret)
                {
                    return 0;
                }
                
                offset += ret;
                
                if (data.size() - offset < 1)
                {
                    return 0;
                }
                
                header.messageType = static_cast<MessageType>(*(data.data() + offset));
                offset += 1;

#ifdef DEBUG
                std::cout << ", message type: ";

                switch (header.messageType)
                {
                    case rtmp::MessageType::SET_CHUNK_SIZE: std::cout << "SET_CHUNK_SIZE"; break;
                    case rtmp::MessageType::BYTES_READ: std::cout << "BYTES_READ"; break;
                    case rtmp::MessageType::PING: std::cout << "PING"; break;
                    case rtmp::MessageType::SERVER_BANDWIDTH: std::cout << "SERVER_BANDWIDTH"; break;
                    case rtmp::MessageType::CLIENT_BANDWIDTH: std::cout << "CLIENT_BANDWIDTH"; break;
                    case rtmp::MessageType::AUDIO_PACKET: std::cout << "AUDIO_PACKET"; break;
                    case rtmp::MessageType::VIDEO_PACKET: std::cout << "VIDEO_PACKET"; break;
                    case rtmp::MessageType::FLEX_STREAM: std::cout << "FLEX_STREAM"; break;
                    case rtmp::MessageType::FLEX_OBJECT: std::cout << "FLEX_OBJECT"; break;
                    case rtmp::MessageType::FLEX_MESSAGE: std::cout << "FLEX_MESSAGE"; break;
                    case rtmp::MessageType::NOTIFY: std::cout << "NOTIFY"; break;
                    case rtmp::MessageType::SHARED_OBJ: std::cout << "SHARED_OBJ"; break;
                    case rtmp::MessageType::INVOKE: std::cout << "INVOKE"; break;
                    case rtmp::MessageType::METADATA: std::cout << "METADATA"; break;
                    default: std::cout << "unknown command";
                };
                
                std::cout << "(" << static_cast<uint32_t>(header.messageType) << ")";
#endif
                
                if (header.type != Header::Type::EIGHT_BYTE)
                {
                    // little endian
                    header.messageStreamId = *reinterpret_cast<const uint32_t*>(data.data() + offset);
                    offset += sizeof(header.messageStreamId);

#ifdef DEBUG
                    std::cout << ", message stream ID: " << header.messageStreamId;
#endif
                }
            }
        }

        // extended timestamp
        if (header.ts == 0xFFFFFF)
        {
            uint32_t ret = decodeInt(data, offset, 4, header.timestamp);

            if (!ret)
            {
                return 0;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << ", extended timestamp: " << header.timestamp;
#endif
        }
        else
        {
            header.timestamp = header.ts;
        }

        // relative timestamp
        if (header.type != rtmp::Header::Type::TWELVE_BYTE)
        {
            header.timestamp += previousPackets[header.channel].timestamp;
        }

#ifdef DEBUG
        std::cout << ", final timestamp: " << header.timestamp;
        std::cout << std::endl;
#endif
        
        return offset - originalOffset;
    }
    
    uint32_t decodePacket(const std::vector<uint8_t>& data, uint32_t offset, uint32_t chunkSize, Packet& packet, std::map<uint32_t, rtmp::Header>& previousPackets)
    {
        uint32_t originalOffset = offset;
        
        uint32_t remainingBytes = 0;

        packet.data.clear();

        auto currentPreviousPackets = previousPackets;

        do
        {
            Header header;
            uint32_t ret = decodeHeader(data, offset, header, currentPreviousPackets);
            
            if (!ret)
            {
                return 0;
            }
            
            offset += ret;

            // first header of packer
            if (packet.data.empty())
            {
                packet.header = header;
                remainingBytes = packet.header.length;
            }

            if (header.type == Header::Type::FOUR_BYTE ||
                header.type == Header::Type::EIGHT_BYTE ||
                header.type == Header::Type::TWELVE_BYTE)
            {
                currentPreviousPackets[header.channel] = header;
            }

            uint32_t packetSize = std::min(remainingBytes, chunkSize);

            if (packetSize + offset > data.size())
            {
#ifdef DEBUG
                std::cout << "Not enough data to read" << std::endl;
#endif
                return 0;
            }

            packet.data.insert(packet.data.end(), data.begin() + offset, data.begin() + offset + packetSize);
            
            remainingBytes -= packetSize;
            offset += packetSize;
        }
        while (remainingBytes);

        // store previous packet if successfully read packet
        previousPackets = currentPreviousPackets;
        
        return offset - originalOffset;
    }
    
    static uint32_t encodeHeader(std::vector<uint8_t>& data, Header& header, std::map<uint32_t, rtmp::Header>& previousPackets)
    {
        uint32_t originalSize = static_cast<uint32_t>(data.size());
        
        uint8_t headerData = static_cast<uint8_t>(static_cast<uint8_t>(header.type) << 6);

        if (static_cast<uint32_t>(header.channel) < 64)
        {
            headerData |= static_cast<uint8_t>(header.channel);
            data.push_back(headerData);
        }
        else if (static_cast<uint32_t>(header.channel) < 64 + 256)
        {
            headerData |= 0;
            data.push_back(headerData);
            encodeInt(data, 1, static_cast<uint32_t>(header.channel) - 64);
        }
        else
        {
            headerData |= 1;
            data.push_back(headerData);
            encodeInt(data, 2, static_cast<uint32_t>(header.channel) - 64);
        }

        uint32_t timestamp = header.timestamp;

        bool useDelta = previousPackets[header.channel].messageType != MessageType::NONE &&
            previousPackets[header.channel].messageStreamId != header.messageStreamId &&
            header.timestamp > previousPackets[header.channel].timestamp;

        if (useDelta)
        {
            if (header.messageType == previousPackets[header.channel].messageType &&
                header.length == previousPackets[header.channel].length)
            {
                if (header.timestamp == previousPackets[header.channel].timestamp)
                {
                    header.type = rtmp::Header::Type::ONE_BYTE;
                }
                else
                {
                    header.type = rtmp::Header::Type::FOUR_BYTE;
                }
            }
            else
            {
                header.type = rtmp::Header::Type::EIGHT_BYTE;
            }
        }
        else
        {
            header.type = rtmp::Header::Type::TWELVE_BYTE;
        }

        // relative timestamp
        if (useDelta)
        {
            timestamp -= previousPackets[header.channel].timestamp;
        }

        if (header.type != Header::Type::ONE_BYTE)
        {
            uint32_t ret = encodeInt(data, 3, (timestamp >= 0xffffff) ? 0xffffff : timestamp);
            
            if (!ret)
            {
                return 0;
            }
            
            if (header.type != Header::Type::FOUR_BYTE)
            {
                ret = encodeInt(data, 3, header.length);
                
                if (!ret)
                {
                    return 0;
                }
                
                data.insert(data.end(), static_cast<uint8_t>(header.messageType));
                
                if (header.type != Header::Type::EIGHT_BYTE)
                {
                    // little endian
                    const uint8_t* messageStreamId = reinterpret_cast<const uint8_t*>(&header.messageStreamId);
                    data.insert(data.end(), messageStreamId, messageStreamId + sizeof(uint32_t));
                }
            }
        }

        if (timestamp >= 0xffffff)
        {
            uint32_t ret = encodeInt(data, 4, timestamp);

            if (!ret)
            {
                return 0;
            }
        }
        
        return static_cast<uint32_t>(data.size()) - originalSize;
    }
    
    uint32_t encodePacket(std::vector<uint8_t>& data, uint32_t chunkSize, const Packet& packet, std::map<uint32_t, rtmp::Header>& previousPackets)
    {
        uint32_t originalSize = static_cast<uint32_t>(data.size());

        uint32_t remainingBytes = static_cast<uint32_t>(packet.data.size());
        uint32_t start = 0;

        while (remainingBytes > 0)
        {
            Header header = packet.header;

            if (!encodeHeader(data, header, previousPackets))
            {
                return 0;
            }

            if (header.type == Header::Type::FOUR_BYTE ||
                header.type == Header::Type::EIGHT_BYTE ||
                header.type == Header::Type::TWELVE_BYTE)
            {
                previousPackets[header.channel] = header;
            }

            uint32_t size = std::min(remainingBytes, chunkSize);
            
            data.insert(data.end(), packet.data.begin() + start, packet.data.begin() + start + size);

            start += size;
            remainingBytes -= size;
        }
        
        return static_cast<uint32_t>(data.size()) - originalSize;
    }
}
