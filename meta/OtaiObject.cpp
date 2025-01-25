#include "OtaiObject.h"

#include "swss/logger.h"

#include "otai_serialize.h"

using namespace otaimeta;

OtaiObject::OtaiObject(
        _In_ const otai_object_meta_key_t& metaKey):
    m_metaKey(metaKey)
{
    SWSS_LOG_ENTER();

    if (!otai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type: %d", metaKey.objecttype);
    }
}

otai_object_type_t OtaiObject::getObjectType() const
{
    SWSS_LOG_ENTER();

    return m_metaKey.objecttype;
}

bool OtaiObject::hasAttr(
        _In_ otai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    return m_attrs.find(id) != m_attrs.end();
}

const otai_object_meta_key_t& OtaiObject::getMetaKey() const
{
    SWSS_LOG_ENTER();

    return m_metaKey;
}

void OtaiObject::setAttr(
        _In_ const otai_attr_metadata_t* md,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    m_attrs[attr->id] = std::make_shared<OtaiAttrWrapper>(md, *attr);
}

void OtaiObject::setAttr(
        _In_ std::shared_ptr<OtaiAttrWrapper> attr)
{
    SWSS_LOG_ENTER();

    m_attrs[attr->getAttrId()] = attr;
}

std::shared_ptr<OtaiAttrWrapper> OtaiObject::getAttr(
        _In_ otai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    auto it = m_attrs.find(id);

    if (it != m_attrs.end())
        return it->second;

    return nullptr;
}

std::vector<std::shared_ptr<OtaiAttrWrapper>> OtaiObject::getAttributes() const
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<OtaiAttrWrapper>> values;

    for (auto&kvp: m_attrs)
        values.push_back(kvp.second);

    return values;
}
