#ifndef __SAI_VS_STATE__
#define __SAI_VS_STATE__

#include "meta/sai_meta.h"
#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

#include "swss/selectableevent.h"

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
             * On destructor we need to call free to deallocate possible
             * allocated list on constructor.
             */

            sai_deserialize_free_attribute_value(m_meta->attrvaluetype, m_attr);
        }

        const sai_attribute_t* getAttr() const
        {
            SWSS_LOG_ENTER();

            return &m_attr;
        }

        const sai_attr_metadata_t* getAttrMetadata() const
        {
            SWSS_LOG_ENTER();

            return m_meta;
        }

        const std::string& getAttrStrValue() const
        {
            SWSS_LOG_ENTER();

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
 * @brief AttrHash key is attribute ID, value is actual attribute
 */
typedef std::map<std::string, std::shared_ptr<SaiAttrWrap>> AttrHash;

/**
 * @brief ObjectHash is map indexed by object type and then serialized object id.
 */
typedef std::map<sai_object_type_t, std::map<std::string, AttrHash>> ObjectHash;

#define DEFAULT_VLAN_NUMBER 1

typedef struct _fdb_info_t
{
    sai_object_id_t port_id;

    sai_vlan_id_t vlan_id;

    sai_object_id_t bridge_port_id;

    sai_fdb_entry_t fdb_entry;

    uint32_t timestamp;

    bool operator<(const _fdb_info_t& other) const
    {
        int res = memcmp(fdb_entry.mac_address, other.fdb_entry.mac_address, sizeof(sai_mac_t));

        if (res < 0)
            return true;

        if (res > 0)
            return false;

        return vlan_id < other.vlan_id;
    }

    bool operator() (const _fdb_info_t& lhs, const _fdb_info_t & rhs) const
    {
        return lhs < rhs;
    }

} fdb_info_t;

extern std::string sai_vs_serialize_fdb_info(
        _In_ const fdb_info_t& fdb_info);

extern void sai_vs_deserialize_fdb_info(
        _In_ const std::string& data,
        _Out_ fdb_info_t& fdb_info);

#define SAI_VS_FDB_INFO "SAI_VS_FDB_INFO"

extern std::set<fdb_info_t> g_fdb_info_set;

class SwitchState
{
    public:

        SwitchState(
                _In_ sai_object_id_t switch_id):
            m_switch_id(switch_id)
        {
            SWSS_LOG_ENTER();

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

    std::map<std::string, std::map<int, uint64_t>> countersMap;

    sai_object_id_t getSwitchId() const
    {
        SWSS_LOG_ENTER();

        return m_switch_id;
    }

    bool getRunLinkThread() const
    {
        SWSS_LOG_ENTER();

        return m_run_link_thread;
    }

    void setRunLinkThread(
            _In_ bool run)
    {
        SWSS_LOG_ENTER();

        m_run_link_thread = run;
    }

    swss::SelectableEvent* getLinkThreadEvent()
    {
        SWSS_LOG_ENTER();

        return &m_link_thread_event;
    }

    void setLinkThread(
            _In_ std::shared_ptr<std::thread> thread)
    {
        SWSS_LOG_ENTER();

        m_link_thread = thread;
    }

    std::shared_ptr<std::thread> getLinkThread() const
    {
        SWSS_LOG_ENTER();

        return m_link_thread;
    }

    void setIfNameToPortId(
            _In_ const std::string& ifname,
            _In_ sai_object_id_t port_id)
    {
        SWSS_LOG_ENTER();

        m_ifname_to_port_id_map[ifname] = port_id;
    }

    sai_object_id_t getPortIdFromIfName(
        _In_ const std::string& ifname) const
    {
        SWSS_LOG_ENTER();

        auto it = m_ifname_to_port_id_map.find(ifname);

        if (it == m_ifname_to_port_id_map.end())
        {
            return SAI_NULL_OBJECT_ID;
        }

        return it->second;
    }

    private:

        sai_object_id_t m_switch_id;

        std::map<std::string, sai_object_id_t> m_ifname_to_port_id;

        swss::SelectableEvent m_link_thread_event;

        volatile bool m_run_link_thread;

        std::shared_ptr<std::thread> m_link_thread;

        std::map<std::string, sai_object_id_t> m_ifname_to_port_id_map;
};

typedef std::map<sai_object_id_t, std::shared_ptr<SwitchState>> SwitchStateMap;

extern SwitchStateMap g_switch_state_map;

void vs_reset_id_counter();
void vs_clear_switch_ids();
void vs_free_real_object_id(
        _In_ sai_object_id_t switch_id);

sai_status_t vs_recreate_hostif_tap_interfaces(
        _In_ sai_object_id_t switch_id);

sai_object_id_t vs_create_real_object_id(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id);

void processFdbInfo(
        _In_ const fdb_info_t &fi,
        _In_ sai_fdb_event_t fdb_event);

#endif // __SAI_VS_STATE__
