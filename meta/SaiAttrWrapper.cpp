#include "SaiAttrWrapper.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace saimeta;

SaiAttrWrapper::SaiAttrWrapper(
        _In_ const sai_attr_metadata_t* meta,
        _In_ const sai_attribute_t& attr):
    m_meta(meta),
    m_attr(attr)
{
    SWSS_LOG_ENTER();

    if (!meta)
    {
        SWSS_LOG_THROW("metadata can't be null");
    }

    m_attr.id = attr.id;

    /*
     * We are making serialize and deserialize to get copy of attribute, it may
     * be a list so we need to allocate new memory.
     *
     * This copy will be used later to get previous value of attribute if
     * attribute will be updated. And if this attribute is oid list then we
     * need to release object reference count.
     */

    std::string str = sai_serialize_attr_value(*meta, attr, false);

    sai_deserialize_attr_value(str, *meta, m_attr, false);
}

SaiAttrWrapper::~SaiAttrWrapper()
{
    SWSS_LOG_ENTER();

    /*
     * On destructor we need to call free to deallocate possible allocated
     * memory on attribute value.
     */

    sai_deserialize_free_attribute_value(m_meta->attrvaluetype, m_attr);
}

const sai_attribute_t* SaiAttrWrapper::getSaiAttr() const
{
    SWSS_LOG_ENTER();

    return &m_attr;
}

const sai_attr_metadata_t* SaiAttrWrapper::getSaiAttrMetadata() const
{
    SWSS_LOG_ENTER();

    return m_meta;
}

sai_attr_id_t SaiAttrWrapper::getAttrId() const
{
    SWSS_LOG_ENTER();

    return m_attr.id;
}
