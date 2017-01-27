//
//  rtmp_relay
//

#include "Connection.h"
#include "Relay.h"
#include "Constants.h"
#include "RTMP.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Connection::Connection(cppsocket::Socket& aSocket, Type aType):
        id(Relay::nextId()),
        generator(rd()),
        socket(aSocket),
        type(aType)
    {
        // push handshake
        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Connected to " << ipToString(socket.getIPAddress()) << ":" << socket.getPort();

        std::vector<uint8_t> version;
        version.push_back(RTMP_VERSION);
        socket.send(version);

        rtmp::Challenge challenge;
        challenge.time = 0;
        std::copy(RTMP_SERVER_VERSION, RTMP_SERVER_VERSION + sizeof(RTMP_SERVER_VERSION), challenge.version);

        for (size_t i = 0; i < sizeof(challenge.randomBytes); ++i)
        {
            uint32_t randomValue = std::uniform_int_distribution<uint32_t>{0, 255}(generator);

            challenge.randomBytes[i] = static_cast<uint8_t>(randomValue);
        }

        std::vector<uint8_t> challengeMessage;
        challengeMessage.insert(challengeMessage.begin(),
                                reinterpret_cast<uint8_t*>(&challenge),
                                reinterpret_cast<uint8_t*>(&challenge) + sizeof(challenge));
        socket.send(challengeMessage);
        
        state = State::VERSION_SENT;
    }

    void Connection::update()
    {
    }
}
