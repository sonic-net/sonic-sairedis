#ifndef __SYNCD_SAISWITCH_H__
#define __SYNCD_SAISWITCH_H__

extern "C" {
#include "sai.h"
}

/**
 * @def SAI_DISCOVERY_LIST_MAX_ELEMENTS
 *
 * Defines maximum elements that can be obtained from the OID list when
 * performing list attribute query (discovery) on the switch.
 *
 * This vlaue will be used to allocate memory on the stack for obtaining object
 * list, and should be big enough to obtain list for all ports on the switch
 * and vlan members.
 */
#define SAI_DISCOVERY_LIST_MAX_ELEMENTS 1024

class SaiSwitch
{
    public:

        /*
         * Constructors.
         */

        SaiSwitch(
                _In_ sai_object_id_t switch_vid,
                _In_ sai_object_id_t switch_rid);

        /*
         * Methods.
         */

        sai_object_id_t getVid() const;
        sai_object_id_t getRid() const;

        std::string getHardwareInfo() const;

        /**
         * @brief Indicates whether RID was discovered on switch init.
         *
         * During switch operation some RIDs are removable, like vlan member.
         * If user will remove such RID, then this function will no longer
         * return true for that RID.
         *
         * @param rid Real ID to be examined.
         *
         * @return True if RID was discovered during init.
         */
        bool isDefaultCreatedRid(
                _In_ sai_object_id_t rid) const;

        /**
         * @brief Indicates whether object can't be removed.
         *
         * Checks whether object can be removed. All non discovered obejcts can
         * be removed. All objects from internal attribute can't be removed.
         *
         * Currently there are some hard coded object types that can't be
         * removed like queues, ingress PG, ports. This may not be true for
         * some vendors.
         *
         * @param rid Real obejct ID to be examined.
         *
         * @return True if object can't be removed from switch.
         */
        bool isNonRemovableRid(
                _In_ sai_object_id_t rid) const;

        /**
         * @brief Collect switch counters.
         *
         * Collects supported counters from each port and put them to specified
         * table.
         *
         * @param countersTable Counters table to be used.
         */
        void collectCounters(
                _In_ swss::Table &countersTable) const;

        /*
         * Redis Static Methods.
         */

        static std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetObjectMap(
                _In_ const std::string &key);

        /**
         * @brief Gets existing objects on the switch.
         *
         * This set can be different from discovered objects after switch init
         * when for example default VLAN members will be removed.
         *
         * This set can't grow, but it can be reduced.
         *
         * @returns Default existing objects on the switch.
         */
        std::set<sai_object_id_t> getExistingObjects() const;

        /**
         * @brief Gets default object based on switch attribute.
         *
         * NOTE: This method will throw exception if invalid attribute is
         * specified, since attribute queried by this method are explicitly
         * declared in SaiSwitch constructor.
         *
         * @param attr_id Switch attrbute to query.
         *
         * @return Valid RID or specified switch attribute received from
         * switch.  This value can be also SAI_NULL_OBJECT_ID if switch don't
         * support this attribute.
         */
        sai_object_id_t getSwitchDefaultAttrOid(
                _In_ sai_attr_id_t attr_id) const;

        /**
         * @brief Remove existing object from the switch.
         *
         * An ASIC remove operation is performed.
         * Function throws when object can't be removed.
         *
         * @param rid Real object ID.
         */
        void removeExistingObject(
                _In_ sai_object_id_t rid);

        /**
         * @brief Remove existing object reference only from discovery map.
         *
         * No ASIC operation is performed.
         * Function throws when object was not found.
         *
         * @param rid Real object ID.
         */
        void removeExistingObjectReference(
                _In_ sai_object_id_t rid);

        /**
         * @brief Gets switch default MAC address.
         *
         * @param[out] mac MAC address to be obtained.
         */
        void getDefaultMacAddress(
                _Out_ sai_mac_t& mac);

        /**
         * @brief Gets default value of attribute for given object.
         *
         * This applies to objects discovered after switch init like
         * SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID.
         *
         * If object or attribute is not found, SAI_NULL_OBJECT_ID is returned.
         */
        sai_object_id_t getDefaultValueForOidAttr(
                _In_ sai_object_id_t rid,
                _In_ sai_attr_id_t attr_id);

    private:

        /*
         * Constructors.
         */

        SaiSwitch(const SaiSwitch&);

        /*
         * Operators.
         */

        SaiSwitch& operator=(const SaiSwitch&);

        /*
         * Fields.
         */

        /**
         * @brief Switch virtual ID assigned by syncd.
         */
        sai_object_id_t m_switch_vid;

        /**
         * @brief Swtich real ID assigned by SAI SDK.
         */
        sai_object_id_t m_switch_rid;

        std::string m_hardware_info;

        sai_mac_t m_default_mac_address;

        std::vector<sai_port_stat_t> m_supported_counters;

        /*
         * NOTE: Those default value will make sense only when we will do hard
         * reinit, since when doing warm restart syncd will restart but if for
         * example we removed some scheduler groups or added/removed ports,
         * those numbers won't match and we will throw.
         *
         * For that case we need special handling. We will implement that
         * later, when this scenario will happen.
         */

        /**
         * @brief Map of default RIDs retrived from Switch object.
         *
         * It will contain RIDs like CPU port, default virtual router, default
         * trap group. etc. Those objects here should be considered non
         * removable.
         */
        std::map<sai_attr_id_t,sai_object_id_t> m_default_rid_map;

        /**
         * @brief Discovered objects.
         *
         * Set of object IDs discovered after calling saiDiscovery method.
         * This set will contain all objects present on the switch right after
         * switch init.
         */
        std::set<sai_object_id_t> m_discovered_rids;

        /*
         * SAI Methods.
         */

        sai_uint32_t saiGetPortCount() const;

        std::string saiGetHardwareInfo() const;

        std::vector<sai_object_id_t> saiGetPortList() const;

        std::unordered_map<sai_uint32_t, sai_object_id_t> saiGetHardwareLaneMap() const;

        std::vector<sai_port_stat_t> saiGetSupportedCounters() const;

        /**
         * @brief Get MAC address.
         *
         * Intended use is to get switch default MAC addres, for comparison
         * logic, when we will try to bring it's default value, in case user
         * changed original switch MAC address.
         *
         * @param[out] mac Obtained MAC address.
         */
        void saiGetMacAddress(
                _Out_ sai_mac_t &mac) const;

        /*
         * Redis Methods.
         */

        std::unordered_map<sai_uint32_t, sai_object_id_t> redisGetLaneMap() const;

    public:

        std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetVidToRidMap() const;

        std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetRidToVidMap() const;

    private:

        void redisSaveLaneMap(
                _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t> &map) const;

        void redisClearLaneMap() const;

        void redisSetDummyAsicStateForRealObjectId(
                _In_ sai_object_id_t rid) const;

        /*
         * Helper Methods.
         */

        void helperCheckLaneMap();

        sai_object_id_t helperGetSwitchAttrOid(
                _In_ sai_attr_id_t attr_id);

        /**
         * @brief Discover objects on the swtich.
         *
         * Method will query recursivly all OID attributes (oid and list) on
         * the given object.
         *
         * This method should be called only once inside constructor right
         * after switch has beed breated to obtain actual ASIC view.
         *
         * @param rid Object to discover other objects.
         * @param processed Set of already processed objects. This set will be
         * updated every time new object ID is discovered.
         */
        void saiDiscover(
                _In_ sai_object_id_t rid,
                _Inout_ std::set<sai_object_id_t> &processed);

        /**
         * @brief Discover helper.
         *
         * Method will call saiDiscovery and collect all discovered objects.
         */
        void helperDiscover();

        void helperPutDiscoveredRidsToRedis();

        void helperInternalOids();

        /*
         * Other Methods.
         */

        std::string getRedisLanesKey() const;
        std::string getRedisHiddenKey() const;

        /**
         * @brief Default oid map.
         *
         * This map will contain default created objects and all their "oid"
         * attributes and it's default value. This will be needed for bringing
         * default values.
         *
         * TODO later on we need to make this for all attributes.
         *
         * Example:
         * SAI_OBJECT_TYPE_SCHEDULER: oid:0x16
         *
         * SAI_OBJECT_TYPE_SCHEDULER_GROUP: oid:0x17
         *     SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID: oid:0x16
         *
         * m_defaultOidMap[0x17][SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID] == 0x16
         */
        std::unordered_map<sai_object_id_t, std::unordered_map<sai_attr_id_t, sai_object_id_t>> m_defaultOidMap;
};

extern std::map<sai_object_id_t, std::shared_ptr<SaiSwitch>> switches;

#endif // __SYNCD_SAISWITCH_H__
