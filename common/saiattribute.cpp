#include "saiattribute.h"

#include "swss/logger.h"
#include "saiserialize.h"

#include "string.h"
extern "C" {
#include "sai.h"
}

SaiAttribute::SaiAttribute(
        _In_ const sai_object_type_t objectType,
        _In_ const swss::FieldValueTuple &value,
        _In_ bool onlyCount) :
    m_onlyCount(onlyCount),
    m_objectType(objectType)
{
    // TODO save those to make possible for copy constructor and assignment
    const std::string &strAttrId = fvField(value);
    const std::string &strAttrValue = fvValue(value);

    if (strAttrId == "NULL")
    {
        SWSS_LOG_ERROR("NULL attribute passed");

        exit(EXIT_FAILURE);
    }

    memset(&m_attr, 0, sizeof(sai_attribute_t));

    int index = 0;
    sai_deserialize_primitive(strAttrId, index, m_attr.id);

    sai_status_t status = sai_get_serialization_type(m_objectType, m_attr.id, m_serializationType);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get serialization type for object type %lx, attribute id: %lx",
                m_objectType,
                m_attr.id);

        exit(EXIT_FAILURE);
    }

    index = 0;
    status = sai_deserialize_attr_value(strAttrValue, index, m_serializationType, m_attr, m_onlyCount);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to deserialize attribute value: %s, serialization type: %d",
                strAttrValue.c_str(),
                m_serializationType);

        exit(EXIT_FAILURE);
    }
}

SaiAttribute::~SaiAttribute()
{
    sai_status_t status = sai_deserialize_free_attribute_value(m_serializationType, m_attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to deserialize free");

        exit(EXIT_FAILURE);
    }
}

sai_attribute_t* SaiAttribute::getAttr()
{
    // reference to member, we may need to
    // replace VID to RID or vice versa
    return &m_attr;
}
