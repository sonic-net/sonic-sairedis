#include "SaiObject.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace saimeta;

SaiObject::SaiObject(
        _In_ const sai_object_meta_key_t& metaKey):
    m_metaKey(metaKey)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_object_type_t SaiObject::getObjectType() const
{
    SWSS_LOG_ENTER();

    return m_metaKey.objecttype;
}

bool SaiObject::hasAttr(
        _In_ sai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    return m_attrs.find(id) != m_attrs.end();
}

const sai_object_meta_key_t& SaiObject::getMetaKey() const
{
    SWSS_LOG_ENTER();

    return m_metaKey;
}

void SaiObject::setAttr(
        _In_ const sai_attr_metadata_t* md,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    m_attrs[attr->id] = std::make_shared<SaiAttrWrapper>(md, *attr);
}

void SaiObject::setAttr(
        _In_ std::shared_ptr<SaiAttrWrapper> attr)
{
    SWSS_LOG_ENTER();

    m_attrs[attr->getAttrId()] = attr;
}

std::shared_ptr<SaiAttrWrapper> SaiObject::getAttr(
        _In_ sai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    auto it = m_attrs.find(id);

    if (it != m_attrs.end())
        return it->second;

    return nullptr;
}

std::vector<std::shared_ptr<SaiAttrWrapper>> SaiObject::getAttributes() const
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<SaiAttrWrapper>> values;

    for (auto&kvp: m_attrs)
        values.push_back(kvp.second);

    return values;
}
