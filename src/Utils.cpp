//
//  rtmp_relay
//

#include "Utils.h"

static size_t replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if(from.empty())
        return 0;

    size_t count = 0;

    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos)
    {
        str.replace(startPos, from.length(), to);
        startPos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'

        ++count;
    }

    return count;
}

size_t replaceTokens(std::string& str, const std::map<std::string, std::string>& tokens)
{
    size_t count = 0;

    for (auto i : tokens)
    {
        count += replaceAll(str, "${" + i.first + "}", i.second);
    }

    return count;
}
