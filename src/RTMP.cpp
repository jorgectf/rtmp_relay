//
//  rtmp_relay
//

#include <iostream>
#include <algorithm>
#include <cmath>
#include "Log.hpp"
#include "RTMP.hpp"
#include "Utils.hpp"

using namespace cppsocket;

namespace relay
{
    namespace rtmp
    {
        static std::string messageTypeToString(MessageType messageType)
        {
            switch (messageType)
            {
                case rtmp::MessageType::SET_CHUNK_SIZE: return "SET_CHUNK_SIZE";
                case rtmp::MessageType::ABORT: return "ABORT";
                case rtmp::MessageType::BYTES_READ: return "BYTES_READ";
                case rtmp::MessageType::USER_CONTROL: return "USER_CONTROL";
                case rtmp::MessageType::SERVER_BANDWIDTH: return "SERVER_BANDWIDTH";
                case rtmp::MessageType::CLIENT_BANDWIDTH: return "CLIENT_BANDWIDTH";
                case rtmp::MessageType::AUDIO_PACKET: return "AUDIO_PACKET";
                case rtmp::MessageType::VIDEO_PACKET: return "VIDEO_PACKET";
                case rtmp::MessageType::AMF3_DATA: return "AMF3_DATA";
                case rtmp::MessageType::AMF3_SHARED_OBJECT: return "AMF3_SHARED_OBJECT";
                case rtmp::MessageType::AMF3_INVOKE: return "AMF3_INVOKE";
                case rtmp::MessageType::AMF0_DATA: return "AMF0_DATA";
                case rtmp::MessageType::AMF0_SHARED_OBJECT: return "AMF0_SHARED_OBJECT";
                case rtmp::MessageType::AMF0_INVOKE: return "AMF0_INVOKE";
                default: return "unknown";
            };
        }

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
                uint32_t ret = decodeIntBE(data, offset, header.channel + 1, newChannel);

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

            log << "(" << static_cast<uint32_t>(header.type) << "), channel: " << static_cast<uint32_t>(header.channel);

            header.length  = previousPackets[header.channel].length;
            header.messageType  = previousPackets[header.channel].messageType;
            header.messageStreamId = previousPackets[header.channel].messageStreamId;
            header.ts = previousPackets[header.channel].ts;

            if (header.type != Header::Type::ONE_BYTE)
            {
                uint32_t ret = decodeIntBE(data, offset, 3, header.ts);

                if (!ret)
                {
                    return 0;
                }

                offset += ret;

                log << ", ts: " << header.ts;

                if (header.ts == 0xffffff)
                {
                    log << " (extended)";
                }

                if (header.type != Header::Type::FOUR_BYTE)
                {
                    ret = decodeIntBE(data, offset, 3, header.length);

                    if (!ret)
                    {
                        return 0;
                    }

                    offset += ret;

                    log << ", data length: " << header.length;

                    if (data.size() - offset < 1)
                    {
                        return 0;
                    }

                    header.messageType = static_cast<MessageType>(*(data.data() + offset));
                    offset += 1;

                    log << ", message type: " << messageTypeToString(header.messageType) << "(" << static_cast<uint32_t>(header.messageType) << ")";

                    if (header.type != Header::Type::EIGHT_BYTE)
                    {
                        if (data.size() - offset < 4)
                        {
                            return 0;
                        }

                        ret = decodeIntLE(data, offset, 4, header.messageStreamId);

                        if (!ret)
                        {
                            return 0;
                        }

                        offset += ret;

                        log << ", message stream ID: " << header.messageStreamId;
                    }
                }
            }

            // extended timestamp
            if (header.ts == 0xffffff)
            {
                uint32_t ret = decodeIntBE(data, offset, 4, header.timestamp);

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

        uint32_t Packet::decode(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t chunkSize, std::map<uint32_t, rtmp::Header>& previousPackets)
        {
            uint32_t originalOffset = offset;

            uint32_t remainingBytes = 0;

            data.clear();

            auto currentPreviousPackets = previousPackets;

            bool firstPacket = true;

            do
            {
                Header header;
                uint32_t ret = decodeHeader(buffer, offset, header, currentPreviousPackets);

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
                    channel = header.channel;
                    messageType = header.messageType;
                    messageStreamId = header.messageStreamId;
                    timestamp = header.timestamp;

                    remainingBytes = header.length;

                    currentPreviousPackets[header.channel].ts = header.ts;
                    currentPreviousPackets[header.channel].timestamp = header.timestamp;

                    firstPacket = false;
                }

                uint32_t packetSize = std::min(remainingBytes, chunkSize);

                if (packetSize + offset > buffer.size())
                {
                    Log(Log::Level::ALL) << "Not enough data to read";

                    return 0;
                }

                data.insert(data.end(), buffer.begin() + offset, buffer.begin() + offset + packetSize);

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
                encodeIntBE(data, 1, header.channel - 64);
            }
            else
            {
                headerData |= 1;
                data.push_back(headerData);
                encodeIntBE(data, 2, header.channel - 64);
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

            log << "(" << static_cast<uint32_t>(header.type) << "), channel: " << static_cast<uint32_t>(header.channel);

            if (header.type != Header::Type::ONE_BYTE)
            {
                uint32_t ret = encodeIntBE(data, 3, header.ts);

                if (!ret)
                {
                    return 0;
                }

                log << ", ts: " << header.ts;

                if (header.ts == 0xffffff)
                {
                    log << " (extended)";
                }

                if (header.type != Header::Type::FOUR_BYTE)
                {
                    ret = encodeIntBE(data, 3, header.length);

                    if (!ret)
                    {
                        return 0;
                    }

                    data.insert(data.end(), static_cast<uint8_t>(header.messageType));

                    log << ", data length: " << header.length;
                    log << ", message type: " << messageTypeToString(header.messageType) << "(" << static_cast<uint32_t>(header.messageType) << ")";

                    if (header.type != Header::Type::EIGHT_BYTE)
                    {
                        ret = encodeIntLE(data, 4, header.messageStreamId);

                        if (!ret)
                        {
                            return 0;
                        }

                        log << ", message stream ID: " << header.messageStreamId;
                    }
                }
            }

            if (header.ts == 0xffffff || (header.type == Header::Type::ONE_BYTE && previousPackets[header.channel].ts == 0xffffff))
            {
                uint32_t ret = encodeIntBE(data, 4, timestamp);

                if (!ret)
                {
                    return 0;
                }

                log << ", extended timestamp: " << header.timestamp;
            }

            log << ", final timestamp: " << header.timestamp;

            return static_cast<uint32_t>(data.size()) - originalSize;
        }

        uint32_t Packet::encode(std::vector<uint8_t>& buffer, uint32_t chunkSize, std::map<uint32_t, rtmp::Header>& previousPackets) const
        {
            uint32_t originalSize = static_cast<uint32_t>(buffer.size());

            uint32_t remainingBytes = static_cast<uint32_t>(data.size());
            uint32_t start = 0;

            Header header;
            header.channel = channel;
            header.messageType = messageType;
            header.messageStreamId = messageStreamId;
            header.timestamp = timestamp;
            header.length = static_cast<uint32_t>(data.size());

            while (remainingBytes > 0)
            {
                if (!encodeHeader(buffer, header, previousPackets))
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

                buffer.insert(buffer.end(), data.begin() + start, data.begin() + start + size);

                start += size;
                remainingBytes -= size;
            }

            return static_cast<uint32_t>(buffer.size()) - originalSize;
        }
    }
}
