#include "Globals.h"

#include "sai_serialize.h"

using namespace saimeta;

std::string Globals::getAttrInfo(
        _In_ const sai_attr_metadata_t& md)
{
    SWSS_LOG_ENTER();

    /*
     * Attribute name will contain object type as well so we don't need to
     * serialize object type separately.
     */

    return std::string(md.attridname) + ":" + sai_serialize_attr_value_type(md.attrvaluetype);
}

std::string Globals::getHardwareInfo(
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

     auto *attr = sai_metadata_get_attr_by_id(
             SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO,
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
         SWSS_LOG_WARN("SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.list is NULL! but count is %u", s8list.count);

         return "";
     }

     uint32_t count = s8list.count;

     if (count > SAI_MAX_HARDWARE_ID_LEN)
     {
         SWSS_LOG_WARN("SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.count (%u) > SAI_MAX_HARDWARE_ID_LEN (%d), LIMITING !!",
                 count,
                 SAI_MAX_HARDWARE_ID_LEN);

         count = SAI_MAX_HARDWARE_ID_LEN;
     }

     // check actual length, since buffer may contain nulls
     auto actualLength = strnlen((const char*)s8list.list, count);

     if (actualLength != count)
     {
         SWSS_LOG_WARN("SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.list is null padded");
     }

     return std::string((const char*)s8list.list, actualLength);
}
