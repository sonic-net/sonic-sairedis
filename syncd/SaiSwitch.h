#pragma once

extern "C" {
#include "sai.h"
}

#include "SaiInterface.h"
#include "VirtualOidTranslator.h"
#include "RedisClient.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <memory>

namespace syncd
{
    class SaiSwitch
    {
        private:

            SaiSwitch(const SaiSwitch&);
            SaiSwitch& operator=(const SaiSwitch&);

        public:

            SaiSwitch(
                    _In_ sai_object_id_t switch_vid,
                    _In_ sai_object_id_t switch_rid,
                    _In_ std::shared_ptr<RedisClient> client,
                    _In_ std::shared_ptr<VirtualOidTranslator> translator,
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ bool warmBoot = false);

            virtual ~SaiSwitch() = default;

        public:

            sai_object_id_t getVid() const;
            sai_object_id_t getRid() const;

            sai_switch_type_t getSwitchType() const;
            std::string getHardwareInfo() const;

            std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const;

            std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const;

            /**
             * @brief Indicates whether RID was discovered on switch init.
             *
             * During switch operation some RIDs are removable, like vlan member.
             * If user will remove such RID, then this function will no longer
             * return true for that RID.
             *
             * If in WARM boot mode this function will also return true for objects
             * that were user created and present on the switch during init.
             *
             * @param rid Real ID to be examined.
             *
             * @return True if RID was discovered during init.
             */
            bool isDiscoveredRid(
                    _In_ sai_object_id_t rid) const;

            /**
             * @brief Indicates whether RID was discovered on switch init at cold boot.
             *
             * During switch operation some RIDs are removable, like vlan member.
             * If user will remove such RID, then this function will no longer
             * return true for that RID.
             *
             * @param rid Real ID to be examined.
             *
             * @return True if RID was discovered during cold boot init.
             */
            bool isColdBootDiscoveredRid(
                    _In_ sai_object_id_t rid) const;

            /**
             * @brief Indicates whether RID is one of default switch objects
             * like CPU port, default virtual router etc.
             *
             * @param rid Real object id to examine.
             *
             * @return True if object is default switch object.
             */
            bool isSwitchObjectDefaultRid(
                    _In_ sai_object_id_t rid) const;

            /**
             * @brief Indicates whether object can't be removed.
             *
             * Checks whether object can be removed. All non discovered objects can
             * be removed. All objects from internal attribute can't be removed.
             *
             * Currently there are some hard coded object types that can't be
             * removed like queues, ingress PG, ports. This may not be true for
             * some vendors.
             *
             * @param rid Real object ID to be examined.
             *
             * @return True if object can't be removed from switch.
             */
            bool isNonRemovableRid(
                    _In_ sai_object_id_t rid) const;

            /*
             * Redis Static Methods.
             */

            /**
             * @brief Gets discovered objects on the switch.
             *
             * This set can be different from discovered objects after switch init
             * when for example default VLAN members will be removed.
             *
             * This set can't grow, but it can be reduced.
             *
             * Also if in WARM boot mode it can contain user created objects.
             *
             * @returns Discovered objects during switch init.
             */
            std::set<sai_object_id_t> getDiscoveredRids() const;

            /**
             * @brief Gets default object based on switch attribute.
             *
             * NOTE: This method will throw exception if invalid attribute is
             * specified, since attribute queried by this method are explicitly
             * declared in SaiSwitch constructor.
             *
             * @param attr_id Switch attribute to query.
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
                    _Out_ sai_mac_t& mac) const;

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

            /**
             * @brief Get cold boot discovered VIDs.
             *
             * @return Set of cold boot discovered VIDs after cold boot.
             */
            std::set<sai_object_id_t> getColdBootDiscoveredVids() const;

            /**
             * @brief Get warm boot discovered VIDs.
             *
             * @return Set of warm boot discovered VIDs after warm boot.
             */
            std::set<sai_object_id_t> getWarmBootDiscoveredVids() const;

            /**
             * @brief On post port create.
             *
             * Performs actions needed after port creation. Will discover new
             * queues, ipgs and scheduler groups that belong to new created port,
             * and updated ASIC DB accordingly.
             */
            void onPostPortCreate(
                    _In_ sai_object_id_t port_rid,
                    _In_ sai_object_id_t port_vid);

            /**
             * @brief Post port remove.
             *
             * Performs actions after port remove. Will remove lanes associated
             * with port from redis lane map.
             */
            void postPortRemove(
                    _In_ sai_object_id_t portRid);

            void collectPortRelatedObjects(
                    _In_ sai_object_id_t portRid);

        private:

            /*
             * SAI Methods.
             */

            sai_uint32_t saiGetPortCount() const;

            std::string saiGetHardwareInfo() const;

            std::vector<sai_object_id_t> saiGetPortList() const;

            std::unordered_map<sai_uint32_t, sai_object_id_t> saiGetHardwareLaneMap() const;

            /**
             * @brief Get port lanes for specific port.
             *
             * @param port_rid Port RID for which lanes should be retrieved.
             *
             * @returns Lanes vector.
             */
            std::vector<uint32_t> saiGetPortLanes(
                    _In_ sai_object_id_t port_rid);

            /**
             * @brief Get MAC address.
             *
             * Intended use is to get switch default MAC address, for comparison
             * logic, when we will try to bring it's default value, in case user
             * changed original switch MAC address.
             *
             * @param[out] mac Obtained MAC address.
             */
            void saiGetMacAddress(
                    _Out_ sai_mac_t &mac) const;

        private:

            void redisSetDummyAsicStateForRealObjectId(
                    _In_ sai_object_id_t rid) const;

            /**
             * @brief Put cold boot discovered VIDs to redis DB.
             *
             * This method will only be called after cold boot and it will save
             * only VIDs that are present on the switch after switch is initialized
             * so it will contain only discovered objects. In case of warm boot
             * this method will not be called.
             */
            void redisSaveColdBootDiscoveredVids() const;

            /**
             * @brief Update lane map for specific port.
             *
             * @param port_rid Port RID for which lane map should be updated
             */
            void redisUpdatePortLaneMap(
                    _In_ sai_object_id_t port_rid);

            /*
             * Helper Methods.
             */

            void helperCheckLaneMap();

            sai_object_id_t helperGetSwitchAttrOid(
                    _In_ sai_attr_id_t attr_id);

            /**
             * @brief Discover helper.
             *
             * Method will call saiDiscovery and collect all discovered objects.
             */
            void helperDiscover();

            void helperSaveDiscoveredObjectsToRedis();

            void helperInternalOids();

            void helperLoadColdVids();

            void helperPopulateWarmBootVids();

            /*
             * Other Methods.
             */

            bool isWarmBoot() const;

            void checkWarmBootDiscoveredRids();

        private:

            /**
             * @brief Switch virtual ID assigned by syncd.
             */
            sai_object_id_t m_switch_vid;

            /**
             * @brief Switch real ID assigned by SAI SDK.
             */
            sai_object_id_t m_switch_rid;

            std::string m_hardware_info;

            sai_mac_t m_default_mac_address;

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
             * @brief Map of default RIDs retrieved from Switch object.
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
             *
             * This set depending on the boot, can contain user created objects if
             * switch was in WARM boot mode. This set can also change if user
             * decides to remove some objects like VLAN_MEMBER.
             */
            std::set<sai_object_id_t> m_discovered_rids;

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

            std::set<sai_object_id_t> m_coldBootDiscoveredVids;

            std::set<sai_object_id_t> m_warmBootDiscoveredVids;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            bool m_warmBoot;

            std::map<sai_object_id_t, std::set<sai_object_id_t>> m_portRelatedObjects;

            std::shared_ptr<VirtualOidTranslator> m_translator;

            std::shared_ptr<RedisClient> m_client;
    };
}
