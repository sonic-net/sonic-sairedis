#include "Workaround.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

using namespace syncd;

/**
 * @brief Determines whether attribute is "workaround" attribute for SET API.
 *
 * Some attributes are not supported on SET API on different platforms.
 * For example SAI_SWITCH_ATTR_SRC_MAC_ADDRESS.
 *
 * @param[in] objectType Object type.
 * @param[in] attrId Attribute Id.
 * @param[in] status Status from SET API.
 *
 * @return True if error from SET API can be ignored, false otherwise.
 */
bool Workaround::isSetAttributeWorkaround(
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    if (status == SAI_STATUS_SUCCESS)
    {
        return false;
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH &&
            attrId == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
    {
        SWSS_LOG_WARN("setting %s failed: %s, not all platforms support this attribute",
                sai_metadata_get_attr_metadata(objectType, attrId)->attridname,
                sai_serialize_status(status).c_str());

        return true;
    }

    return false;
}
