#include "SaiAttr.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace syncd;

SaiAttr::SaiAttr(
        _In_ const std::string &str_attr_id,
        _In_ const std::string &str_attr_value):
    m_str_attr_id(str_attr_id),
    m_str_attr_value(str_attr_value),
    m_meta(NULL)
{
    SWSS_LOG_ENTER();

    /*
     * We perform deserialize here to have attribute value when we need
     * it, this can include allocated lists, so on destructor we need
     * to free this memory.
     */

    sai_deserialize_attr_id(str_attr_id, &m_meta);

    m_attr.id = m_meta->attrid;

    sai_deserialize_attr_value(str_attr_value, *m_meta, m_attr, false);

    if (m_meta->isenum && m_meta->enummetadata->ignorevalues)
    {
        // if attribute is enum, and we are loading some older SAI values it
        // may happen that we get depreacated/ignored value string and we need
        // to update it to current one to not cause attribute compare confusion
        // since they are compared by string value

        auto val = sai_serialize_enum(m_attr.value.s32, m_meta->enummetadata);

        SWSS_LOG_NOTICE("translating depreacated/ignore enum %s to %s",
                m_str_attr_value.c_str(),
                val.c_str());

        m_str_attr_value = val;
    }
}

SaiAttr::~SaiAttr()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_attribute_value(m_meta->attrvaluetype, m_attr);
}

sai_attribute_t* SaiAttr::getRWSaiAttr()
{
    SWSS_LOG_ENTER();

    return &m_attr;
}

const sai_attribute_t* SaiAttr::getSaiAttr() const
{
    SWSS_LOG_ENTER();

    return &m_attr;
}

sai_object_id_t SaiAttr::getOid() const
{
    SWSS_LOG_ENTER();

    if (m_meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
    {
        return m_attr.value.oid;
    }

    if (m_meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID &&
            m_attr.value.aclaction.enable)
    {
        return m_attr.value.aclaction.parameter.oid;
    }

    SWSS_LOG_THROW("attribute %s is not OID attribute", m_meta->attridname);
}

bool SaiAttr::isObjectIdAttr() const
{
    SWSS_LOG_ENTER();

    return m_meta->isoidattribute;
}

const std::string& SaiAttr::getStrAttrId() const
{
    SWSS_LOG_ENTER();

    return m_str_attr_id;
}

const std::string& SaiAttr::getStrAttrValue() const
{
    SWSS_LOG_ENTER();

    return m_str_attr_value;
}

const sai_attr_metadata_t* SaiAttr::getAttrMetadata() const
{
    SWSS_LOG_ENTER();

    return m_meta;
}

void SaiAttr::updateValue()
{
    SWSS_LOG_ENTER();

    m_str_attr_value = sai_serialize_attr_value(*m_meta, m_attr);
}

std::vector<sai_object_id_t> SaiAttr::getOidListFromAttribute() const
{
    SWSS_LOG_ENTER();

    const sai_attribute_t &attr = m_attr;

    uint32_t count = 0;

    const sai_object_id_t *objectIdList = NULL;

    /*
     * For ACL fields and actions we need to use enable flag as
     * indicator, since when attribute is disabled then parameter can
     * be garbage.
     */

    switch (m_meta->attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:

            count = 1;
            objectIdList = &attr.value.oid;

            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:

            count = attr.value.objlist.count;
            objectIdList = attr.value.objlist.list;

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:

            if (attr.value.aclfield.enable)
            {
                count = 1;
                objectIdList = &attr.value.aclfield.data.oid;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

            if (attr.value.aclfield.enable)
            {
                count = attr.value.aclfield.data.objlist.count;
                objectIdList = attr.value.aclfield.data.objlist.list;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:

            if (attr.value.aclaction.enable)
            {
                count = 1;
                objectIdList = &attr.value.aclaction.parameter.oid;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

            if (attr.value.aclaction.enable)
            {
                count = attr.value.aclaction.parameter.objlist.count;
                objectIdList = attr.value.aclaction.parameter.objlist.list;
            }

            break;

        default:

            if (m_meta->isoidattribute)
            {
                SWSS_LOG_THROW("attribute %s is oid attrubute but not handled", m_meta->attridname);
            }

            // Attribute not contain any object ids.

            break;
    }

    std::vector<sai_object_id_t> result(objectIdList, objectIdList + count);

    return result;
}
