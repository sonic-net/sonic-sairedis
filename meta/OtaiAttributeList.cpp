#include "OtaiAttributeList.h"

#include "otai_serialize.h"

using namespace otaimeta;

OtaiAttributeList::OtaiAttributeList(
        _In_ const otai_object_type_t objectType,
        _In_ const std::vector<swss::FieldValueTuple> &values,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    size_t attr_count = values.size();

    for (size_t i = 0; i < attr_count; ++i)
    {
        const std::string &str_attr_id = fvField(values[i]);
        const std::string &str_attr_value = fvValue(values[i]);

        if (str_attr_id == "NULL")
        {
            continue;
        }

        otai_attribute_t attr;
        memset(&attr, 0, sizeof(otai_attribute_t));

        otai_deserialize_attr_id(str_attr_id, attr.id);

        // TODO object type is not necessary, we can use get attr metadata from attr id name
        auto meta = otai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("FATAL: failed to find metadata for object type %d and attr id %d", objectType, attr.id);
        }

        otai_deserialize_attr_value(str_attr_value, *meta, attr, countOnly);

        m_attr_list.push_back(attr);
        m_attr_value_type_list.push_back(meta->attrvaluetype);
    }
}

OtaiAttributeList::OtaiAttributeList(
        _In_ const otai_object_type_t objectType,
        _In_ const std::unordered_map<std::string, std::string>& hash,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    for (auto it = hash.begin(); it != hash.end(); it++)
    {
        const std::string &str_attr_id = it->first;
        const std::string &str_attr_value = it->second;

        if (str_attr_id == "NULL")
        {
            continue;
        }

        otai_attribute_t attr;
        memset(&attr, 0, sizeof(otai_attribute_t));

        otai_deserialize_attr_id(str_attr_id, attr.id);

        // TODO object type is not necessary, we can use get attr metadata from attr id name
        auto meta = otai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("FATAL: failed to find metadata for object type %d and attr id %d", objectType, attr.id);
        }

        otai_deserialize_attr_value(str_attr_value, *meta, attr, countOnly);

        m_attr_list.push_back(attr);
        m_attr_value_type_list.push_back(meta->attrvaluetype);
    }
}

OtaiAttributeList::~OtaiAttributeList()
{
    SWSS_LOG_ENTER();

    size_t attr_count = m_attr_list.size();

    for (size_t i = 0; i < attr_count; ++i)
    {
        otai_attribute_t &attr = m_attr_list[i];

        otai_attr_value_type_t serialization_type = m_attr_value_type_list[i];

        otai_deserialize_free_attribute_value(serialization_type, attr);
    }
}

std::vector<swss::FieldValueTuple> OtaiAttributeList::serialize_attr_list(
        _In_ otai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t index = 0; index < attr_count; ++index)
    {
        const otai_attribute_t *attr = &attr_list[index];

        auto meta = otai_metadata_get_attr_metadata(objectType, attr->id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("FATAL: failed to find metadata for object type %d and attr id %d", objectType, attr->id);
        }

        std::string str_attr_id = otai_serialize_attr_id(*meta);

        std::string str_attr_value = otai_serialize_attr_value(*meta, *attr, countOnly);

        swss::FieldValueTuple fvt(str_attr_id, str_attr_value);

        entry.push_back(fvt);
    }

    return entry;
}

otai_attribute_t* OtaiAttributeList::get_attr_list()
{
    SWSS_LOG_ENTER();

    return m_attr_list.data();
}

uint32_t OtaiAttributeList::get_attr_count()
{
    SWSS_LOG_ENTER();

    return (uint32_t)m_attr_list.size();
}
