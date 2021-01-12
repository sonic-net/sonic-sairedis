#include "swss/logger.h"

int main()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_WARN("syslog sairedis warning");

    SWSS_LOG_ERROR("syslog sairedis error");

    return 0;
}
