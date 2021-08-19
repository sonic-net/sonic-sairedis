#include "SelectableFd.h"

#include "swss/logger.h"

using namespace saivs;

SelectableFd::SelectableFd(
        _In_ int fd)
{
    SWSS_LOG_ENTER();

    if (fd < 0)
    {
        SWSS_LOG_THROW("invalid file descriptor: %d", fd);
    }

    m_fd = fd;
}

int SelectableFd::getFd()
{
    SWSS_LOG_ENTER();

    return m_fd;
}

uint64_t SelectableFd::readData()
{
    SWSS_LOG_ENTER();

    return 0;
}
