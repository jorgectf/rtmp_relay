//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Input.h"
#include "Constants.h"

Input::Input(Network& network, Socket socket):
    _network(network), _socket(std::move(socket)), _generator(_rd())
{
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
    _socket.startRead();
}

Input::~Input()
{
    
}

Input::Input(Input&& other):
    _network(other._network),
    _socket(std::move(other._socket)),
    _data(std::move(other._data)),
    _state(other._state),
    _generator(std::move(other._generator))
{
    other._state = State::UNINITIALIZED;
    
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
}

Input& Input::operator=(Input&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    _state = other._state;
    _generator = std::move(other._generator);
    
    other._state = State::UNINITIALIZED;
    
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
    
    return *this;
}

void Input::update()
{
    
}

bool Input::getPacket(std::vector<uint8_t>& packet)
{
    if (_data.size())
    {
        packet = _data;
        return true;
    }
    
    return false;
}

void Input::handleRead(const std::vector<uint8_t>& data)
{
    _data.insert(_data.end(), data.begin(), data.end());
    
    std::cout << "Input got " << std::to_string(data.size()) << " bytes" << std::endl;
    
    uint32_t offset = 0;
    
    while (offset < _data.size())
    {
        if (_state == State::UNINITIALIZED)
        {
            if (_data.size() - offset >= sizeof(uint8_t))
            {
                // C0
                uint8_t version = static_cast<uint8_t>(*(_data.data() + offset));
                offset += sizeof(version);
                std::cout << "Got version " << version << std::endl;
                
                if (version != 0x03)
                {
                    std::cerr << "Unsuported version" << std::endl;
                    _socket.close();
                    break;
                }
                
                // S0
                std::vector<uint8_t> reply;
                reply.push_back(RTMP_VERSION);
                _socket.send(reply);
                
                _state = State::VERSION_SENT;
            }
            else
            {
                break;
            }
        }
        else if (_state == State::VERSION_SENT)
        {
            if (_data.size() - offset >= sizeof(Challange))
            {
                // C1
                Challange* challange = reinterpret_cast<Challange*>(_data.data() + offset);
                offset += sizeof(*challange);
                
                std::cout << "Got Challange message, time: " << challange->time <<
                    ", version: " << static_cast<uint32_t>(challange->version[0]) << "." <<
                    static_cast<uint32_t>(challange->version[1]) << "." <<
                    static_cast<uint32_t>(challange->version[2]) << "." <<
                    static_cast<uint32_t>(challange->version[3]) << std::endl;
                
                // S1
                Challange replyChallange;
                replyChallange.time = 0;
                memcpy(replyChallange.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
                
                for (size_t i = 0; i < sizeof(replyChallange.randomBytes); ++i)
                {
                    replyChallange.randomBytes[i] = static_cast<uint8_t>(_generator());
                }
                
                std::vector<uint8_t> reply;
                reply.insert(reply.begin(),
                             reinterpret_cast<uint8_t*>(&replyChallange),
                             reinterpret_cast<uint8_t*>(&replyChallange) + sizeof(replyChallange));
                _socket.send(reply);
                
                // S2
                Ack ack;
                ack.time = challange->time;
                ack.time2 = static_cast<uint32_t>(time(nullptr));
                memcpy(ack.randomBytes, challange->randomBytes, sizeof(ack.randomBytes));
                
                std::vector<uint8_t> ackData;
                ackData.insert(ackData.begin(),
                               reinterpret_cast<uint8_t*>(&ack),
                               reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                _socket.send(ackData);
                
                _state = State::ACK_SENT;
            }
            else
            {
                break;
            }
        }
        else  if (_state == State::ACK_SENT)
        {
            if (_data.size() - offset >= sizeof(Ack))
            {
                // C2
                Ack* ack = reinterpret_cast<Ack*>(_data.data() + offset);
                offset += sizeof(*ack);
                
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                
                std::cout << "Handshake done" << std::endl;
                
                _state = State::HANDSHAKE_DONE;
            }
            else
            {
                break;
            }
        }
        else if (_state == State::HANDSHAKE_DONE)
        {
            // send subscribe
            FILE* tmp = fopen("/Users/elviss/Desktop/data.txt", "wb");
            fwrite(_data.data() + offset, 1, _data.size() - offset, tmp);
            fclose(tmp);
            
            offset += _data.size() - offset;
            
            break;
        }
    }
    
    _data.erase(_data.begin(), _data.begin() + offset);
}

void Input::handleClose()
{
    std::cout << "Input disconnected" << std::endl;
}
