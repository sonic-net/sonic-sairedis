#include "OtaiAttrWrapper.h"

#include "swss/logger.h"

#include "otai_serialize.h"

using namespace otaimeta;

OtaiAttrWrapper::OtaiAttrWrapper(
        _In_ const otai_attr_metadata_t* meta,
        _In_ const otai_attribute_t& attr):
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

    std::string str = otai_serialize_attr_value(*meta, attr, false);

    otai_deserialize_attr_value(str, *meta, m_attr, false);
}

OtaiAttrWrapper::~OtaiAttrWrapper()
{
    SWSS_LOG_ENTER();

    /*
     * On destructor we need to call free to deallocate possible allocated
     * memory on attribute value.
     */

    otai_deserialize_free_attribute_value(m_meta->attrvaluetype, m_attr);
}

const otai_attribute_t* OtaiAttrWrapper::getOtaiAttr() const
{
    SWSS_LOG_ENTER();

    return &m_attr;
}

const otai_attr_metadata_t* OtaiAttrWrapper::getOtaiAttrMetadata() const
{
    SWSS_LOG_ENTER();

    return m_meta;
}

otai_attr_id_t OtaiAttrWrapper::getAttrId() const
{
    SWSS_LOG_ENTER();

    return m_attr.id;
}
