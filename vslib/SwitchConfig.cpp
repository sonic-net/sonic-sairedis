#include "saivs.h"
#include "SwitchConfig.h"

#include "swss/logger.h"

#include <boost/algorithm/string/join.hpp>

#include <cstring>

using namespace saivs;

SwitchConfig::SwitchConfig(
        _In_ uint32_t switchIndex,
        _In_ const std::string& hwinfo):
    m_saiSwitchType(SAI_SWITCH_TYPE_NPU),
    m_switchType(SAI_VS_SWITCH_TYPE_NONE),
    m_bootType(SAI_VS_BOOT_TYPE_COLD),
    m_switchIndex(switchIndex),
    m_hardwareInfo(hwinfo),
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
    else if (st == SAI_VALUE_SAI_SWITCH_TYPE_POE)
    {
        saiSwitchType = SAI_SWITCH_TYPE_POE;
    }
    else
    {
        SWSS_LOG_ERROR("unknown SAI switch type: '%s', expected (%s|%s|%s)",
                saiSwitchTypeStr,
                SAI_VALUE_SAI_SWITCH_TYPE_NPU,
                SAI_VALUE_SAI_SWITCH_TYPE_POE,
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
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_BCM56971B0)
    {
        switchType = SAI_VS_SWITCH_TYPE_BCM56971B0;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_BCM81724)
    {
        switchType = SAI_VS_SWITCH_TYPE_BCM81724;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_POE)
    {
        switchType = SAI_VS_SWITCH_TYPE_POE;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_MLNX2700)
    {
        switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_NVDA_MBF2H536C)
    {
        switchType = SAI_VS_SWITCH_TYPE_NVDA_MBF2H536C;
    }
    else if (st == SAI_VALUE_VS_SWITCH_TYPE_DPU_SIMU_2P)
    {
        /*
         * TODO: Temporarily set switchType to SAI_VS_SWITCH_TYPE_NVDA_MBF2H536C
         * for 2-port DPU. This will need to be revisited when there are other
         * DPU types.
         */
        switchType = SAI_VS_SWITCH_TYPE_NVDA_MBF2H536C;
    }
    else
    {
        std::vector<std::string> vals {
                SAI_VALUE_VS_SWITCH_TYPE_BCM81724,
                SAI_VALUE_VS_SWITCH_TYPE_BCM56850,
                SAI_VALUE_VS_SWITCH_TYPE_BCM56971B0,
                SAI_VALUE_VS_SWITCH_TYPE_POE,
                SAI_VALUE_VS_SWITCH_TYPE_MLNX2700,
                SAI_VALUE_VS_SWITCH_TYPE_NVDA_MBF2H536C,
                SAI_VALUE_VS_SWITCH_TYPE_DPU_SIMU_2P
        };

        SWSS_LOG_ERROR("unknown switch type: '%s', expected (%s)",
                switchTypeStr,
                boost::algorithm::join(vals, "|").c_str());

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
        return strcmp(useTapDeviceStr, "true") == 0;
    }

    return false;
}
