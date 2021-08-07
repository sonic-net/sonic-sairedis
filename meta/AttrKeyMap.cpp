#include "AttrKeyMap.h"

#include "sai_serialize.h"

using namespace saimeta;

void AttrKeyMap::clear()
{
    SWSS_LOG_ENTER();

    m_map.clear();
}

void AttrKeyMap::insert(
        _In_ const std::string& metaKey,
        _In_ const std::string& attrKey)
{
    SWSS_LOG_ENTER();

    m_map[metaKey] = attrKey;
}


void AttrKeyMap::eraseMetaKey(
        _In_ const std::string& metaKey)
{
    SWSS_LOG_ENTER();

    auto it = m_map.find(metaKey);

    if (it != m_map.end())
    {
        SWSS_LOG_DEBUG("erasing attributes key %s", it->second.c_str());

        m_map.erase(it);
    }
}

bool AttrKeyMap::attrKeyExists(
        _In_ const std::string& attrKey) const
{
    SWSS_LOG_ENTER();

    for (auto& it: m_map)
    {
        if (it.second == attrKey)
        {
            return true;
        }
    }

    return false;
}

std::string AttrKeyMap::constructKey(
        _In_ sai_object_id_t switchId,
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    if (switchId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("switchId is NULL for %s",
                sai_serialize_object_meta_key(metaKey).c_str());
    }

    // Use map to make sure that keys will be always sorted by attr id.

    std::map<int32_t, std::string> keys;

    for (uint32_t idx = 0; idx < attrCount; ++idx)
    {
        const auto& attr = attrList[idx];

        auto* md = sai_metadata_get_attr_metadata(metaKey.objecttype, attr.id);

        if (!md)
        {
            SWSS_LOG_THROW("failed to get metadata for object type: %s and attr id: %d",
                    sai_serialize_object_id(metaKey.objecttype).c_str(),
                    attr.id);
        }

        const auto& value = attr.value;

        if (!SAI_HAS_FLAG_KEY(md->flags))
        {
            continue;
        }

        std::string name = md->attridname + std::string(":");

        switch (md->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_UINT32_LIST: // only for port lanes

                // NOTE: this list should be sorted

                for (uint32_t i = 0; i < value.u32list.count; ++i)
                {
                    name += std::to_string(value.u32list.list[i]);

                    if (i != value.u32list.count - 1)
                    {
                        name += ",";
                    }
                }

                break;

            case SAI_ATTR_VALUE_TYPE_INT32:
                name += std::to_string(value.s32); // if enum then get enum name?
                break;

            case SAI_ATTR_VALUE_TYPE_UINT32:
                name += std::to_string(value.u32);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT8:
                name += std::to_string(value.u8);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT16:
                name += std::to_string(value.u16);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                name += sai_serialize_object_id(value.oid);
                break;

            default:

                // NOTE: only primitive types should be considered as keys
                SWSS_LOG_THROW("FATAL: attribute %s marked as key, but have invalid serialization type, FIXME",
                        md->attridname);
        }

        keys[md->attrid] = name;
    }

    // switch ID is added, since same key pattern is allowed on different switch objects

    std::string key = sai_serialize_object_id(switchId) + ";";

    for (auto& k: keys)
    {
        key += k.second + ";";
    }

    SWSS_LOG_DEBUG("constructed key: %s", key.c_str());

    return key;
}

std::vector<std::string> AttrKeyMap::getAllKeys() const
{
    SWSS_LOG_ENTER();

    std::vector<std::string> vec;

    for (auto& it: m_map)
    {
        vec.push_back(it.first);
    }

    return vec;
}

