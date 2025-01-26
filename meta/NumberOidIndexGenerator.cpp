#include "NumberOidIndexGenerator.h"

#include "swss/logger.h"

using namespace otaimeta;

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

void NumberOidIndexGenerator::reset()
{
    SWSS_LOG_ENTER();

    m_index = 0;
}
