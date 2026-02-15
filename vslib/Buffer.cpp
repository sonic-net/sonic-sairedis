#include "Buffer.h"

#include "swss/logger.h"

#include <cstring>

using namespace saivs;

Buffer::Buffer(
        _In_ const uint8_t* data,
        _In_ size_t size)
{
    SWSS_LOG_ENTER();

    if (!data)
    {
        SWSS_LOG_THROW("data is NULL!");
    }

    m_data = std::vector<uint8_t>(data, data + size);
}

const uint8_t* Buffer::getData() const
{
    SWSS_LOG_ENTER();

    return m_data.data();
}

size_t Buffer::getSize() const
{
    SWSS_LOG_ENTER();

    return m_data.size();
}
