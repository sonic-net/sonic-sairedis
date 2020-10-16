#include "saivs.h"
#include "SwitchConfig.h"

#include "swss/logger.h"

using namespace saivs;

SwitchConfig::SwitchConfig():
    m_saiSwitchType(SAI_SWITCH_TYPE_NPU),
    m_switchType(SAI_VS_SWITCH_TYPE_NONE),
    m_bootType(SAI_VS_BOOT_TYPE_COLD),
    m_switchIndex(0),
    m_hardwareInfo(""),
    m_useTapDevice(false)
{
    SWSS_LOG_ENTER();

    // empty
}


bool SwitchConfig::parseSaiSwitchType(
        _In_ const char* saiSwitchTypeStr,
        _Out_ sai_switch_type_t& saiSwitchType)
{
    SWSS_LOG_ENTER();

    std::string st = (saiSwitchTypeStr == NULL) ? "unknown" : saiSwitchTypeStr;

    if (st == SAI_VALUE_SAI_SWITCH_TYPE_NPU)
    {
        saiSwitchType = SAI_SWITCH_TYPE_NPU;
    }
    else if (st == SAI_VALUE_SAI_SWITCH_TYPE_PHY)
    {
        saiSwitchType = SAI_SWITCH_TYPE_PHY;
    }
    else
    {
        SWSS_LOG_ERROR("unknown SAI switch type: '%s', expected (%s|%s)",
                saiSwitchTypeStr,
                SAI_VALUE_SAI_SWITCH_TYPE_NPU,
                SAI_VALUE_SAI_SWITCH_TYPE_PHY);

        return false;
    }

    return true;
}

bool SwitchConfig::parseSwitchType(
        _In_ const char* switchTypeStr,
        _Out_ sai_vs_switch_type_t& switchType)
{
    SWSS_LOG_ENTER();

    std::string st = (switchTypeStr == NULL) ? "unknown" : switchTypeStr;

    if (st == SAI_VALUE_VS_SWITCH_TYPE_BCM56850)
    {
        switchType = SAI_VS_SWITCH_TYPE_BCM56850;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_BCM81724)
    {
        switchType = SAI_VS_SWITCH_TYPE_BCM81724;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_MLNX2700)
    {
        switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    }
    else
    {
        SWSS_LOG_ERROR("unknown switch type: '%s', expected (%s|%s|%s)",
                switchTypeStr,
                SAI_VALUE_VS_SWITCH_TYPE_BCM81724,
                SAI_VALUE_VS_SWITCH_TYPE_BCM56850,
                SAI_VALUE_VS_SWITCH_TYPE_MLNX2700);

        return false;
    }

    return true;
}

bool SwitchConfig::parseBootType(
        _In_ const char* bootTypeStr,
        _Out_ sai_vs_boot_type_t& bootType)
{
    SWSS_LOG_ENTER();

    std::string bt = (bootTypeStr == NULL) ? "cold" : bootTypeStr;

    if (bt == "cold" || bt == SAI_VALUE_VS_BOOT_TYPE_COLD)
    {
        bootType = SAI_VS_BOOT_TYPE_COLD;
    }
    else if (bt == "warm" || bt == SAI_VALUE_VS_BOOT_TYPE_WARM)
    {
        bootType = SAI_VS_BOOT_TYPE_WARM;
    }
    else if (bt == "fast" || bt == SAI_VALUE_VS_BOOT_TYPE_FAST)
    {
        bootType = SAI_VS_BOOT_TYPE_FAST;
    }
    else
    {
        SWSS_LOG_ERROR("unknown boot type: '%s', expected (cold|warm|fast)", bootTypeStr);

        return false;
    }

    return true;
}

bool SwitchConfig::parseUseTapDevice(
        _In_ const char* useTapDeviceStr)
{
    SWSS_LOG_ENTER();

    if (useTapDeviceStr)
    {
        std::string utd = useTapDeviceStr;

        return utd == "true";
    }

    return false;
}
