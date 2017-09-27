#ifndef __SAI_VS_STATE__
#define __SAI_VS_STATE__

#include "meta/sai_meta.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

#include <unordered_map>
#include <string>
#include <set>

#define CHECK_STATUS(status)            \
    {                                   \
        sai_status_t s = (status);      \
        if (s != SAI_STATUS_SUCCESS)    \
            { return s; }               \
    }

// TODO unify wrapper and add to common
class SaiAttrWrap
{
    public:

        SaiAttrWrap(
                _In_ sai_object_type_t object_type,
                _In_ const sai_attribute_t *attr)
        {
            SWSS_LOG_ENTER();

            m_meta = sai_metadata_get_attr_metadata(object_type, attr->id);

            m_attr.id = attr->id;

            /*
             * We are making serialize and deserialize to get copy of
             * attribute, it may be a list so we need to allocate new memory.
             *
             * This copy will be used later to get previous value of attribute
             * if attribute will be updated. And if this attribute is oid list
             * then we need to release object reference count.
             */

            m_value = sai_serialize_attr_value(*m_meta, *attr, false);

            sai_deserialize_attr_value(m_value, *m_meta, m_attr, false);
        }

        ~SaiAttrWrap()
        {
            SWSS_LOG_ENTER();

            /*
             * On destructor we need to call free to dealocate possible
             * alocated list on constructor.
             */

            sai_deserialize_free_attribute_value(m_meta->attrvaluetype, m_attr);
        }

        const sai_attribute_t* getAttr() const
        {
            return &m_attr;
        }

        const sai_attr_metadata_t* getAttrMetadata() const
        {
            return m_meta;
        }

        const std::string& getAttrStrValue() const
        {
            return m_value;
        }

    private:

        SaiAttrWrap(const SaiAttrWrap&);
        SaiAttrWrap& operator=(const SaiAttrWrap&);

        const sai_attr_metadata_t *m_meta;
        sai_attribute_t m_attr;

        std::string m_value;
};

/**
 * @brief AttrHash key is attribute ID, value is actuall attribute
 */
typedef std::map<std::string, std::shared_ptr<SaiAttrWrap>> AttrHash;

/**
 * @brief ObjectHash is map indexed by object type and then serialized object id.
 */
typedef std::map<sai_object_type_t, std::map<std::string, AttrHash>> ObjectHash;

#define DEFAULT_VLAN_NUMBER 1

class SwitchState
{
    public:

        SwitchState(
                _In_ sai_object_id_t switch_id):
            m_switch_id(switch_id)
        {
            if (sai_object_type_query(switch_id) != SAI_OBJECT_TYPE_SWITCH)
            {
                SWSS_LOG_THROW("object %s is not SWITCH, its %s",
                        sai_serialize_object_id(switch_id).c_str(),
                        sai_serialize_object_type(sai_object_type_query(switch_id)).c_str());
            }

            for (int i = SAI_OBJECT_TYPE_NULL; i < (int)SAI_OBJECT_TYPE_MAX; ++i)
            {
                /*
                 * Populate empty maps for each object to avoid checking if
                 * objecttype exists.
                 */

                objectHash[(sai_object_type_t)i] = { };
            }

            /*
             * Create switch by default, it will require special treat on
             * creating.
             */

            objectHash[SAI_OBJECT_TYPE_SWITCH][sai_serialize_object_id(switch_id)] = {};
        }

    ObjectHash objectHash;

    sai_object_id_t getSwitchId() const
    {
    return m_switch_id;
    }

    private:

        sai_object_id_t m_switch_id;
};

typedef std::map<sai_object_id_t, std::shared_ptr<SwitchState>> SwitchStateMap;

extern SwitchStateMap g_switch_state_map;

void vs_reset_id_counter();
void vs_clear_switch_ids();
void vs_free_real_object_id(
        _In_ sai_object_id_t switch_id);

sai_object_id_t vs_create_real_object_id(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id);

#endif // __SAI_VS_STATE__
