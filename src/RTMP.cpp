//
//  rtmp_relay
//

#include <iostream>
#include "RTMP.h"

namespace rtmp
{
    static uint32_t parseInt(const std::vector<uint8_t>& data, uint32_t offset, uint32_t size, uint32_t& result)
    {
        if (data.size() - offset < size)
        {
            return 0;
        }
        
        result = 0;
        
        for (uint32_t i = 0; i < size; ++i)
        {
            result <<= 1;
            result += static_cast<uint32_t>(*(data.data() + offset));
            offset += 1;
        }
        
        return size;
    }
    
    uint32_t parseHeader(const std::vector<uint8_t>& data, uint32_t offset, Header& header)
    {
        uint32_t originalOffset = offset;
        
        if (data.size() - offset < 1)
        {
            return 0;
        }
        
        uint8_t headerData = *(data.data() + offset);
        offset += 1;
        
        if ((headerData & 0x3F) != 0x03)
        {
            std::cerr << "Wrong header version" << std::endl;
            return 0;
        }
        
        header.type = static_cast<HeaderType>(headerData >> 6);
        
        if (header.type != HeaderType::ONE_BYTE)
        {
            uint32_t ret = parseInt(data, offset, 3, header.timestamp);
            
            if (!ret)
            {
                return 0;
            }
            
            offset += ret;
            
            std::cout << "Timestamp: " << header.timestamp << std::endl;
            
            if (header.type != HeaderType::FOUR_BYTE)
            {
                if (data.size() - offset < 4)
                {
                    return 0;
                }
                
                ret = parseInt(data, offset, 3, header.length);
                
                if (!ret)
                {
                    return 0;
                }
                
                offset += ret;
                
                std::cout << "Length: " << header.length << std::endl;
                
                if (data.size() - offset < 1)
                {
                    return 0;
                }
                
                header.messageType = static_cast<MessageType>(*(data.data() + offset));
                offset += 1;
                
                std::cout << "Message type ID: " << static_cast<uint32_t>(header.messageType) << std::endl;
                
                if (header.type != HeaderType::EIGHT_BYTE)
                {
                    header.messageStreamId = *reinterpret_cast<const uint32_t*>(data.data() + offset);
                    offset += sizeof(header.messageStreamId);
                    
                    std::cout << "Message stream ID: " << header.messageStreamId << std::endl;
                }
            }
        }
        
        return offset - originalOffset;
    }
    
    uint32_t parsePacket(const std::vector<uint8_t>& data, uint32_t offset, uint32_t chunkSize, Packet& packet)
    {
        uint32_t originalOffset = offset;
        
        uint32_t remainingBytes = 0;
        
        do
        {
            Header header;
            uint32_t ret = parseHeader(data, offset, header);
            
            if (!ret)
            {
                return 0;
            }
            
            offset += ret;
            
            if (packet.data.empty())
            {
                packet.header = header;
                remainingBytes = packet.header.length - static_cast<uint32_t>(packet.data.size());
            }
            
            if (offset - data.size() < remainingBytes)
            {
                return 0;
            }
            else
            {
                uint32_t packetSize = (remainingBytes > chunkSize ? chunkSize : remainingBytes);
                
                std::cout << "Packet size: " << packetSize << std::endl;
                
                packet.data.insert(packet.data.end(), data.begin() + offset, data.begin() + offset + packetSize);
                
                remainingBytes -= packetSize;
                offset += packetSize;
            }
            
        }
        while (remainingBytes);
        
        return offset - originalOffset;
    }
}
