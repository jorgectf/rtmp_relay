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
    uint32_t decodeHeader(const std::vector<uint8_t>& data, uint32_t offset, Header& header, Packet& previousPacket)
    {
        uint32_t originalOffset = offset;
        
        if (data.size() - offset < 1)
        {
            return 0;
        }
        
        uint8_t headerData = *(data.data() + offset);
        offset += 1;

        uint32_t channel = static_cast<uint32_t>(headerData & 0x3F);
        header.type = static_cast<Header::Type>(headerData >> 6);

        if (channel == 0)
        {
            if (data.size() - offset < 1)
            {
                return 0;
            }

            channel = 64;
            channel += static_cast<uint32_t>(*(data.data() + offset));
            offset += 1;
        }
        else if (channel == 1)
        {
            if (data.size() - offset < 2)
            {
                return 0;
            }

            channel = 64;
            channel += static_cast<uint32_t>(*(data.data() + offset));
            channel += 256 * static_cast<uint32_t>(*(data.data() + offset));
            offset += 2;
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

        header.channel = static_cast<Channel>(channel);

        std::cout << "(" << static_cast<uint32_t>(header.type) << ")";

        std::cout << ", channel: ";

        switch (header.channel)
        {
            case Channel::NETWORK: std::cout << "NETWORK"; break;
            case Channel::SYSTEM: std::cout << "SYSTEM"; break;
            case Channel::AUDIO: std::cout << "AUDIO"; break;
            case Channel::VIDEO: std::cout << "VIDEO"; break;
            case Channel::SOURCE: std::cout << "SOURCE"; break;
            default: std::cout << "invalid channel"; break;
        };

        std::cout << "(" << static_cast<uint32_t>(header.channel) << ")";
#endif
        
        if (header.type != Header::Type::ONE_BYTE)
        {
            uint32_t ret = decodeInt(data, offset, 3, header.timestamp);

#ifdef DEBUG
            std::cout << ", timestamp: " << header.timestamp;
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
        if (header.type == Header::Type::ONE_BYTE &&
            (previousPacket.header.type == Header::Type::TWELVE_BYTE ||
             previousPacket.header.type == Header::Type::EIGHT_BYTE ||
             previousPacket.header.type == Header::Type::FOUR_BYTE) &&
            previousPacket.header.timestamp == 0x00FFFFFF)
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

#ifdef DEBUG
        std::cout << std::endl;
#endif
        
        return offset - originalOffset;
    }
    
    uint32_t decodePacket(const std::vector<uint8_t>& data, uint32_t offset, uint32_t chunkSize, Packet& packet, Packet& previousPacket)
    {
        uint32_t originalOffset = offset;
        
        uint32_t remainingBytes = 0;

        packet.data.clear();

        do
        {
            Header header;
            uint32_t ret = decodeHeader(data, offset, header, previousPacket);
            
            if (!ret)
            {
                return 0;
            }
            
            offset += ret;
            
            if (packet.data.empty())
            {
                packet.header = header;
                remainingBytes = packet.header.length;

                if (packet.header.type == Header::Type::TWELVE_BYTE ||
                    packet.header.type == Header::Type::EIGHT_BYTE ||
                    packet.header.type == Header::Type::FOUR_BYTE)
                {
                    previousPacket = packet;
                }
            }
            
            if (offset - data.size() < remainingBytes)
            {
                return 0;
            }
            else
            {
                uint32_t packetSize = std::min(remainingBytes, chunkSize);
                
                packet.data.insert(packet.data.end(), data.begin() + offset, data.begin() + offset + packetSize);
                
                remainingBytes -= packetSize;
                offset += packetSize;
            }
            
        }
        while (remainingBytes);
        
        return offset - originalOffset;
    }
    
    uint32_t encodeHeader(std::vector<uint8_t>& data, const Header& header)
    {
        uint32_t originalSize = static_cast<uint32_t>(data.size());
        
        uint8_t headerData = static_cast<uint8_t>(header.channel);
        headerData |= (static_cast<uint8_t>(header.type) << 6);

        data.push_back(headerData);
        
        if (header.type != Header::Type::ONE_BYTE)
        {
            uint32_t ret = encodeInt(data, 3, header.timestamp);
            
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
        
        return static_cast<uint32_t>(data.size()) - originalSize;
    }
    
    uint32_t encodePacket(std::vector<uint8_t>& data, uint32_t chunkSize, const Packet& packet)
    {
        uint32_t originalSize = static_cast<uint32_t>(data.size());

        uint32_t remainingBytes = static_cast<uint32_t>(packet.data.size());
        uint32_t start = 0;

        while (remainingBytes > 0)
        {
            Header header;

            if (remainingBytes == static_cast<uint32_t>(packet.data.size()))
            {
                header = packet.header;

                if (header.length == 0)
                {
                    header.length = static_cast<uint32_t>(packet.data.size());
                }
            }
            else
            {
                header.type = Header::Type::ONE_BYTE;
                header.channel = packet.header.channel;
            }

            encodeHeader(data, header);

            uint32_t size = std::min(remainingBytes, chunkSize);
            
            data.insert(data.end(), packet.data.begin() + start, packet.data.begin() + start + size);

            start += size;
            remainingBytes -= size;
        }
        
        return static_cast<uint32_t>(data.size()) - originalSize;
    }
}
