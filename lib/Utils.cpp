#include "Utils.h"

extern "C" {
#include "saimetadata.h"
}

#include "meta/sai_serialize.h"
#include "swss/logger.h"

using namespace sairedis;

void Utils::clearOidList(
        _Out_ sai_object_list_t& list)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < list.count; ++idx)
    {
        list.list[idx] = SAI_NULL_OBJECT_ID;
    }
}

void Utils::clearOidValues(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _Out_ sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attrCount; i++)
    {
        sai_attribute_t &attr = attrList[i];

        auto meta = sai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = SAI_NULL_OBJECT_ID;
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                clearOidList(attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                attr.value.aclfield.data.oid = SAI_NULL_OBJECT_ID;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    clearOidList(attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                attr.value.aclaction.parameter.oid = SAI_NULL_OBJECT_ID;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    clearOidList(attr.value.aclaction.parameter.objlist);
                break;

            default:

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is object id, but not processed, FIXME", meta->attridname);
                }

                break;
        }

    }
}
