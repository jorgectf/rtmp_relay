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

std::string getAudioCodec(AudioCodec codecId)
{
    switch (codecId)
    {
        case AudioCodec::ADPCM: return "ADPCM";
        case AudioCodec::MP3: return "MP3";
        case AudioCodec::LINEAR_LE: return "LinearLE";
        case AudioCodec::NELLY16: return "Nellymoser16";
        case AudioCodec::NELLY8: return "Nellymoser8";
        case AudioCodec::NELLY: return "Nellymoser";
        case AudioCodec::G711A: return "G711A";
        case AudioCodec::G711U: return "G711U";
        case AudioCodec::AAC: return "AAC";
        case AudioCodec::SPEEX: return "Speex";
        case AudioCodec::MP3_8: return "MP3-8K";
        case AudioCodec::DEVSPEC: return "DeviceSpecific";
        case AudioCodec::UNCOMPRESSED: return "Uncompressed";
    }

    return "unknown";
}

std::string getVideoCodec(VideoCodec codecId)
{
    switch (codecId)
    {
        case VideoCodec::JPEG: return "Jpeg";
        case VideoCodec::SORENSON_H263: return "Sorenson-H263";
        case VideoCodec::SCREEN: return "ScreenVideo";
        case VideoCodec::ON2_VP6: return "On2-VP6";
        case VideoCodec::ON2_VP6_ALPHA: return "On2-VP6-Alpha";
        case VideoCodec::SCREEN2: return "ScreenVideo2";
        case VideoCodec::H264: return "H264";
    }

    return "unknown";
}
