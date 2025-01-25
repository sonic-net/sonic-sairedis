#include "Globals.h"

#include "otai_serialize.h"

using namespace otaimeta;

std::string Globals::getAttrInfo(
        _In_ const otai_attr_metadata_t& md)
{
    SWSS_LOG_ENTER();

    /*
     * Attribute name will contain object type as well so we don't need to
     * serialize object type separately.
     */

    return std::string(md.attridname) + ":" + otai_serialize_attr_value_type(md.attrvaluetype);
}

std::string Globals::getHardwareInfo(
        _In_ uint32_t attrCount,
        _In_ const otai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    auto *attr = otai_metadata_get_attr_by_id(
            -1,// OTAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO, // NO OTAI attribute
            attrCount,
            attrList);

    if (attr == NULL)
    {
        return "";
    }

    auto& s8list = attr->value.s8list;

    if (s8list.count == 0)
    {
        return "";
    }

    if (s8list.list == NULL)
    {
        SWSS_LOG_WARN("OTAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.list is NULL! but count is %u", s8list.count);

        return "";
    }

    uint32_t count = s8list.count;

#define OTAI_MAX_HARDWARE_ID_LEN 0 // not exists in OTAI

    if (count > OTAI_MAX_HARDWARE_ID_LEN)
    {
        SWSS_LOG_WARN("OTAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.count (%u) > OTAI_MAX_HARDWARE_ID_LEN (%d), LIMITING !!",
                count,
                OTAI_MAX_HARDWARE_ID_LEN);

        count = OTAI_MAX_HARDWARE_ID_LEN;
    }

    // check actual length, since buffer may contain nulls
    auto actualLength = strnlen((const char*)s8list.list, count);

    if (actualLength != count)
    {
        SWSS_LOG_WARN("OTAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.list is null padded");
    }

    return std::string((const char*)s8list.list, actualLength);
}

std::string Globals::joinFieldValues(
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            ss << "|";
        }

        ss << fvField(values[i]) << "=" << fvValue(values[i]);
    }

    return ss.str();
}
