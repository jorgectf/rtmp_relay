//
//  rtmp_relay
//

#include "Stream.h"

namespace relay
{
    Stream::Stream(Connection::StreamType aType,
                   const std::string& aApplicationName,
                   const std::string& aStreamName):
        type(aType),
        applicationName(aApplicationName),
        streamName(aStreamName)
    {
    }
}
