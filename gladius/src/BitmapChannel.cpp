#include "BitmapChannel.h"

#include <utility>

namespace gladius
{
    std::string const & BitmapChannel::getName() const
    {
        return m_name;
    }

    BitmapLayer BitmapChannel::generate(float z_mm, Vector2 pixelSize) const
    {
        return m_generator(z_mm, std::move(pixelSize));
    }
}
