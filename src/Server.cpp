//
//  rtmp_relay
//

#include <queue>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Server.h"
#include "Utils.h"

static const int WAITING_QUEUE_SIZE = 5;

Server::Server()
{
    
}

Server::~Server()
{
    if (_socket > 0) close(_socket);
}

bool Server::init(uint16_t port, const std::vector<std::string>& pushAddresses)
{
    _port = port;
    
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    
    pollfd pollFd;
    pollFd.fd = _socket;
    pollFd.events = POLLIN;
    _pollFds.push_back(pollFd);
    
    if (_socket < 0)
    {
        std::cerr << "Failed to create server socket" << std::endl;
        return false;
    }
    
    if (!setBlocking(_socket, false))
    {
        std::cerr << "Failed to set socket non-blocking" << std::endl;
        return false;
    }
    
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(_socket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        std::cerr << "Failed to bind server socket" << std::endl;
        return false;
    }
    
    if (listen(_socket, WAITING_QUEUE_SIZE) < 0)
    {
        std::cerr << "Failed to listen on port " << _port << std::endl;
        return false;
    }
    
    std::cout << "Server listening on port " << _port << std::endl;
    
    for (const std::string address : pushAddresses)
    {
        std::unique_ptr<Output> output(new Output());
        
        if (output->init(address))
        {
            pollfd pollFd;
            pollFd.fd = output->getSocket();
            pollFd.events = POLLOUT;
            _pollFds.push_back(pollFd);
            
            _outputs.push_back(std::move(output));
        }
    }
    
    return true;
}

void Server::update()
{
    if (poll(_pollFds.data(), static_cast<nfds_t>(_pollFds.size()), 0) < 0)
    {
        std::cerr << "Poll failed" << std::endl;
        return;
    }
    
    std::queue<std::unique_ptr<Input>> inputQueue;
    
    for (std::vector<pollfd>::iterator i = _pollFds.begin(); i != _pollFds.end();)
    {
        pollfd pollFd = (*i);
        
        if (pollFd.revents & POLLIN)
        {
            if (pollFd.fd == _socket)
            {
                std::unique_ptr<Input> input(new Input());
                
                if (input->init(_socket))
                {
                    inputQueue.push(std::move(input));
                }
            }
            else
            {
                std::vector<std::unique_ptr<Input>>::iterator inputIterator =
                    std::find_if(_inputs.begin(), _inputs.end(), [pollFd](const std::unique_ptr<Input>& input) { return input->getSocket() == pollFd.fd; });
                
                std::vector<char> packet;
                
                // Failed to find input
                if (inputIterator != _inputs.end())
                {
                    if ((*inputIterator)->readPacket(packet))
                    {
                        // packet
                        for (const std::unique_ptr<Output>& output : _outputs)
                        {
                            if (output->isConnected())
                            {
                                output->sendPacket(packet);
                            }
                        }
                    }
                    else if ((*inputIterator)->isClosed())
                    {
                        std::cout << "Client disconnected" << std::endl;
                        
                        _inputs.erase(inputIterator);
                        i = _pollFds.erase(i);
                        continue;
                    }
                }
            }
        }
        else if (pollFd.revents & POLLOUT)
        {
            std::vector<std::unique_ptr<Output>>::iterator outputIterator =
            std::find_if(_outputs.begin(), _outputs.end(), [pollFd](const std::unique_ptr<Output>& output) { return output->getSocket() == pollFd.fd; });
            
            if (outputIterator != _outputs.end())
            {
                (*outputIterator)->connected();
                i = _pollFds.erase(i);
                continue;
            }
        }

        ++i;
    }
    
    while (!inputQueue.empty())
    {
        std::unique_ptr<Input> input = std::move(inputQueue.front());
        inputQueue.pop();
        
        pollfd pollFd;
        pollFd.fd = input->getSocket();
        pollFd.events = POLLIN;
        _pollFds.push_back(pollFd);
        
        _inputs.push_back(std::move(input));
    }
}
