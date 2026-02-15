#include "SaiAttributeList.h"

#include "sai_serialize.h"

using namespace saimeta;

SaiAttributeList::SaiAttributeList(
        _In_ const sai_object_type_t objectType,
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

        sai_attribute_t attr;
        memset(&attr, 0, sizeof(sai_attribute_t));

        sai_deserialize_attr_id(str_attr_id, attr.id);

        // TODO object type is not necessary, we can use get attr metadata from attr id name
        auto meta = sai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("FATAL: failed to find metadata for object type %d and attr id %d", objectType, attr.id);
        }

        sai_deserialize_attr_value(str_attr_value, *meta, attr, countOnly);

        m_attr_list.push_back(attr);
        m_attr_value_type_list.push_back(meta->attrvaluetype);
    }
}

SaiAttributeList::SaiAttributeList(
        _In_ const sai_object_type_t objectType,
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

        sai_attribute_t attr;
        memset(&attr, 0, sizeof(sai_attribute_t));

        sai_deserialize_attr_id(str_attr_id, attr.id);

        // TODO object type is not necessary, we can use get attr metadata from attr id name
        auto meta = sai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("FATAL: failed to find metadata for object type %d and attr id %d", objectType, attr.id);
        }

        sai_deserialize_attr_value(str_attr_value, *meta, attr, countOnly);

        m_attr_list.push_back(attr);
        m_attr_value_type_list.push_back(meta->attrvaluetype);
    }
}

SaiAttributeList::~SaiAttributeList()
{
    SWSS_LOG_ENTER();

    size_t attr_count = m_attr_list.size();

    for (size_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &attr = m_attr_list[i];

        sai_attr_value_type_t serialization_type = m_attr_value_type_list[i];

        sai_deserialize_free_attribute_value(serialization_type, attr);
    }
}

std::vector<swss::FieldValueTuple> SaiAttributeList::serialize_attr_list(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t index = 0; index < attr_count; ++index)
    {
        const sai_attribute_t *attr = &attr_list[index];

        auto meta = sai_metadata_get_attr_metadata(objectType, attr->id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("FATAL: failed to find metadata for object type %d and attr id %d", objectType, attr->id);
        }

        std::string str_attr_id = sai_serialize_attr_id(*meta);

        std::string str_attr_value = sai_serialize_attr_value(*meta, *attr, countOnly);

        swss::FieldValueTuple fvt(str_attr_id, str_attr_value);

        entry.push_back(fvt);
    }

    return entry;
}

sai_attribute_t* SaiAttributeList::get_attr_list()
{
    SWSS_LOG_ENTER();

    return m_attr_list.data();
}

uint32_t SaiAttributeList::get_attr_count()
{
    SWSS_LOG_ENTER();

    return (uint32_t)m_attr_list.size();
}
