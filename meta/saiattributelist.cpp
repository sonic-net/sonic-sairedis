#include "saiattributelist.h"
#include "sai_meta.h"
#include "saiserialize.h"

SaiAttributeList::SaiAttributeList(
        _In_ const sai_object_type_t object_type,
        _In_ const std::vector<swss::FieldValueTuple> &values,
        _In_ bool countOnly)
{
    size_t attr_count = values.size();

    for (size_t i = 0; i < attr_count; ++i)
    {
        const std::string &str_attr_id = fvField(values[i]);
        const std::string &str_attr_value = fvValue(values[i]);

        if (str_attr_id == "NULL")
        {
            continue;
        }

        sai_attribute_t attr;
        memset(&attr, 0, sizeof(sai_attribute_t));

        sai_deserialize_attr_id(str_attr_id, attr.id);

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("FATAL: failed to find metadata for object type %d and attr id %d", object_type, attr.id);
            throw std::runtime_error("failed to get metadata");
        }

        sai_deserialize_attr_value(str_attr_value, *meta, attr, countOnly);

        m_attr_list.push_back(attr);
        m_attr_value_type_list.push_back(meta->attrvaluetype);
    }
}

SaiAttributeList::~SaiAttributeList()
{
    size_t attr_count = m_attr_list.size();

    for (size_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &attr = m_attr_list[i];

        sai_attr_value_type_t serialization_type = m_attr_value_type_list[i];

        sai_deserialize_free_attribute_value(serialization_type, attr);
    }
}

std::vector<swss::FieldValueTuple> SaiAttributeList::serialize_attr_list(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool countOnly)
{
    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t index = 0; index < attr_count; ++index)
    {
        const sai_attribute_t *attr = &attr_list[index];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr->id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("FATAL: failed to find metadata for object type %d and attr id %d", object_type, attr->id);
            throw std::runtime_error("failed to get metadata");
        }

        std::string str_attr_id = sai_serialize_attr_id(*meta);

        std::string str_attr_value = sai_serialize_attr_value(*meta, *attr, countOnly);

        swss::FieldValueTuple fvt(str_attr_id, str_attr_value);

        entry.push_back(fvt);
    }

    return std::move(entry);
}

sai_attribute_t* SaiAttributeList::get_attr_list()
{
    return m_attr_list.data();
}

uint32_t SaiAttributeList::get_attr_count()
{
    return (uint32_t)m_attr_list.size();
}
