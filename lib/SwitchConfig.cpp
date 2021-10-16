#include "SwitchConfig.h"

#include "swss/logger.h"

using namespace sairedis;

SwitchConfig::SwitchConfig():
    m_switchIndex(0),
    m_hardwareInfo("")
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchConfig::SwitchConfig(
        _In_ uint32_t switchIndex,
        _In_ const std::string& hwinfo):
    m_switchIndex(switchIndex),
    m_hardwareInfo(hwinfo)
{
    SWSS_LOG_ENTER();

    // empty
}
