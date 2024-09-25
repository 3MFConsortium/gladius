#include "wordwarp.h"

namespace gladius
{
    std::string warpTextAfter(std::string str, size_t wrapAfter)
    {
        for (size_t currentWrapPosition = str.find_first_of(' ', wrapAfter);
             currentWrapPosition != std::string::npos;
             currentWrapPosition = str.find_first_of(' ', currentWrapPosition + wrapAfter))
        {
            str.at(currentWrapPosition) = '\n';
        }
        return str;
    }
}