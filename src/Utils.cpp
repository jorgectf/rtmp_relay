//
//  rtmp_relay
//

#include <fcntl.h>
#include "Utils.h"

bool setBlocking(int fd, bool blocking)
{
#ifdef WIN32
    unsigned long mode = blocking ? 0 : 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
    return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}