//
//  rtmp_relay
//

#include <iostream>
#include <algorithm>
#include <cmath>
#include "Log.h"
#include "RTMP.h"
#include "Utils.h"

using namespace cppsocket;

namespace relay
{
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

            Log log(Log::Level::ALL);
            log << "Header type: ";

            switch (header.type)
            {
                case Header::Type::TWELVE_BYTE: log << "TWELVE_BYTE"; break;
                case Header::Type::EIGHT_BYTE: log << "EIGHT_BYTE"; break;
                case Header::Type::FOUR_BYTE: log << "FOUR_BYTE"; break;
                case Header::Type::ONE_BYTE: log << "ONE_BYTE"; break;
                default: log << "invalid header type"; break;
            };

            log << "(" << static_cast<uint32_t>(header.type) << ")";

            log << ", channel: " << static_cast<uint32_t>(header.channel);

            header.length  = previousPackets[header.channel].length;
            header.messageType  = previousPackets[header.channel].messageType;
            header.messageStreamId = previousPackets[header.channel].messageStreamId;
            header.ts = previousPackets[header.channel].ts;
            
            if (header.type != Header::Type::ONE_BYTE)
            {
                uint32_t ret = decodeInt(data, offset, 3, header.ts);

                log << ", ts: " << header.ts;

                if (header.ts == 0xffffff)
                {
                    log << " (extended)";
                }
                
                if (!ret)
                {
                    return 0;
                }
                
                offset += ret;
                
                if (header.type != Header::Type::FOUR_BYTE)
                {
                    ret = decodeInt(data, offset, 3, header.length);

                    log << ", data length: " << header.length;
                    
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

                    log << ", message type: ";

                    switch (header.messageType)
                    {
                        case rtmp::MessageType::SET_CHUNK_SIZE: log << "SET_CHUNK_SIZE"; break;
                        case rtmp::MessageType::BYTES_READ: log << "BYTES_READ"; break;
                        case rtmp::MessageType::PING: log << "PING"; break;
                        case rtmp::MessageType::SERVER_BANDWIDTH: log << "SERVER_BANDWIDTH"; break;
                        case rtmp::MessageType::CLIENT_BANDWIDTH: log << "CLIENT_BANDWIDTH"; break;
                        case rtmp::MessageType::AUDIO_PACKET: log << "AUDIO_PACKET"; break;
                        case rtmp::MessageType::VIDEO_PACKET: log << "VIDEO_PACKET"; break;
                        case rtmp::MessageType::FLEX_STREAM: log << "FLEX_STREAM"; break;
                        case rtmp::MessageType::FLEX_OBJECT: log << "FLEX_OBJECT"; break;
                        case rtmp::MessageType::FLEX_MESSAGE: log << "FLEX_MESSAGE"; break;
                        case rtmp::MessageType::NOTIFY: log << "NOTIFY"; break;
                        case rtmp::MessageType::SHARED_OBJ: log << "SHARED_OBJ"; break;
                        case rtmp::MessageType::INVOKE: log << "INVOKE"; break;
                        default: log << "unknown command";
                    };
                    
                    log << "(" << static_cast<uint32_t>(header.messageType) << ")";
                    
                    if (header.type != Header::Type::EIGHT_BYTE)
                    {
                        // little endian
                        header.messageStreamId = *reinterpret_cast<const uint32_t*>(data.data() + offset);
                        offset += sizeof(header.messageStreamId);

                        log << ", message stream ID: " << header.messageStreamId;
                    }
                }
            }

            // extended timestamp
            if (header.ts == 0xffffff)
            {
                uint32_t ret = decodeInt(data, offset, 4, header.timestamp);

                if (!ret)
                {
                    return 0;
                }

                offset += ret;

                log << ", extended timestamp: " << header.timestamp;
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

            log << ", final timestamp: " << header.timestamp;
            
            return offset - originalOffset;
        }
        
        uint32_t decodePacket(const std::vector<uint8_t>& data, uint32_t offset, uint32_t chunkSize, Packet& packet, std::map<uint32_t, rtmp::Header>& previousPackets)
        {
            uint32_t originalOffset = offset;
            
            uint32_t remainingBytes = 0;

            packet.data.clear();

            auto currentPreviousPackets = previousPackets;

            bool firstPacket = true;

            do
            {
                Header header;
                uint32_t ret = decodeHeader(data, offset, header, currentPreviousPackets);
                
                if (!ret)
                {
                    return 0;
                }
                
                offset += ret;

                if (header.type == Header::Type::FOUR_BYTE ||
                    header.type == Header::Type::EIGHT_BYTE ||
                    header.type == Header::Type::TWELVE_BYTE)
                {
                    currentPreviousPackets[header.channel] = header;
                }

                // first header of packer
                if (firstPacket)
                {
                    packet.channel = header.channel;
                    packet.messageType = header.messageType;
                    packet.messageStreamId = header.messageStreamId;
                    packet.timestamp = header.timestamp;

                    remainingBytes = header.length;

                    currentPreviousPackets[header.channel].ts = header.ts;
                    currentPreviousPackets[header.channel].timestamp = header.timestamp;

                    firstPacket = false;
                }

                uint32_t packetSize = std::min(remainingBytes, chunkSize);

                if (packetSize + offset > data.size())
                {
                    Log(Log::Level::ALL) << "Not enough data to read";

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

            bool useDelta = previousPackets[header.channel].channel != Channel::NONE &&
                previousPackets[header.channel].messageStreamId == header.messageStreamId &&
                header.timestamp >= previousPackets[header.channel].timestamp;

            uint64_t timestamp = header.timestamp;

            // relative timestamp
            if (useDelta)
            {
                timestamp -= previousPackets[header.channel].timestamp;
            }

            if (timestamp >= 0xffffff)
            {
                header.ts = 0xffffff;
            }
            else
            {
                header.ts = static_cast<uint32_t>(timestamp);
            }

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
            
            uint8_t headerData = static_cast<uint8_t>(static_cast<uint8_t>(header.type) << 6);

            if (header.channel < 64)
            {
                headerData |= static_cast<uint8_t>(header.channel);
                data.push_back(headerData);
            }
            else if (static_cast<uint32_t>(header.channel) < 64 + 256)
            {
                headerData |= 0;
                data.push_back(headerData);
                encodeInt(data, 1, header.channel - 64);
            }
            else
            {
                headerData |= 1;
                data.push_back(headerData);
                encodeInt(data, 2, header.channel - 64);
            }

            if (header.type != Header::Type::ONE_BYTE)
            {
                uint32_t ret = encodeInt(data, 3, header.ts);
                
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

            if (header.ts == 0xffffff || (header.type == Header::Type::ONE_BYTE && previousPackets[header.channel].ts == 0xffffff))
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

            Header header;
            header.channel = packet.channel;
            header.messageType = packet.messageType;
            header.messageStreamId = packet.messageStreamId;
            header.timestamp = packet.timestamp;
            header.length = static_cast<uint32_t>(packet.data.size());

            while (remainingBytes > 0)
            {
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
}
