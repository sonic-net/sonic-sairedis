#include "NumberOidIndexGenerator.h"

#include "swss/logger.h"

using namespace saimeta;

NumberOidIndexGenerator::NumberOidIndexGenerator()
{
    SWSS_LOG_ENTER();

    reset();
}

uint64_t NumberOidIndexGenerator::increment()
{
    SWSS_LOG_ENTER();

    return ++m_index;
}

std::vector<uint64_t> NumberOidIndexGenerator::incrementBy(
    _In_ uint64_t count)
{
    SWSS_LOG_ENTER();

    std::vector<uint64_t> result;
    result.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count; ++i)
    {
        result.push_back(++m_index);
    }

    return result;
}

void NumberOidIndexGenerator::reset()
{
    SWSS_LOG_ENTER();

    m_index = 0;
}
