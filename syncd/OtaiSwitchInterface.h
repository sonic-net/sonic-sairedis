#pragma once

extern "C" {
#include "otai.h"
#include "otaimetadata.h"
}

#include <set>
#include <unordered_map>
#include <map>

namespace syncd
{
    class OtaiSwitchInterface
    {
        private:

            OtaiSwitchInterface(const SaiSwitchInterface&);
            OtaiSwitchInterface& operator=(const SaiSwitchInterface&);

        public:

            OtaiSwitchInterface(
                    _In_ otai_object_id_t switchVid,
                    _In_ otai_object_id_t switchRid);

            virtual ~OtaiSwitchInterface() = default;

        public:

            otai_object_id_t getVid() const;
            otai_object_id_t getRid() const;

        public:

            virtual std::unordered_map<otai_object_id_t, otai_object_id_t> getVidToRidMap() const = 0;

            virtual std::unordered_map<otai_object_id_t, otai_object_id_t> getRidToVidMap() const = 0;

            virtual bool isDiscoveredRid(
                    _In_ otai_object_id_t rid) const = 0;

            virtual bool isColdBootDiscoveredRid(
                    _In_ otai_object_id_t rid) const = 0;

            virtual bool isSwitchObjectDefaultRid(
                    _In_ otai_object_id_t rid) const = 0;

            virtual bool isNonRemovableRid(
                    _In_ otai_object_id_t rid) const = 0;

            virtual std::set<otai_object_id_t> getDiscoveredRids() const = 0;

            /**
             * @brief Gets default object based on switch attribute.
             *
             * NOTE: This method will throw exception if invalid attribute is
             * specified, since attribute queried by this method are explicitly
             * declared in OtaiSwitch constructor.
             *
             * @param attr_id Switch attribute to query.
             *
             * @return Valid RID or specified switch attribute received from
             * switch.  This value can be also SAI_NULL_OBJECT_ID if switch don't
             * support this attribute.
             */
            virtual otai_object_id_t getSwitchDefaultAttrOid(
                    _In_ otai_attr_id_t attr_id) const;


            virtual void removeExistingObject(
                    _In_ otai_object_id_t rid) = 0;

            virtual void removeExistingObjectReference(
                    _In_ otai_object_id_t rid) = 0;

            virtual void getDefaultMacAddress(
                    _Out_ otai_mac_t& mac) const = 0;

            virtual otai_object_id_t getDefaultValueForOidAttr(
                    _In_ otai_object_id_t rid,
                    _In_ otai_attr_id_t attr_id) = 0;

            virtual std::set<otai_object_id_t> getColdBootDiscoveredVids() const = 0;

            virtual std::set<otai_object_id_t> getWarmBootDiscoveredVids() const = 0;

            virtual std::set<otai_object_id_t> getWarmBootNewDiscoveredVids();

            virtual void onPostPortCreate(
                    _In_ otai_object_id_t port_rid,
                    _In_ otai_object_id_t port_vid) = 0;

            virtual void postPortRemove(
                    _In_ otai_object_id_t portRid) = 0;

            virtual void collectPortRelatedObjects(
                    _In_ otai_object_id_t portRid) = 0;

        protected:

            /**
             * @brief Switch virtual ID assigned by syncd.
             */
            otai_object_id_t m_switch_vid;

            /**
             * @brief Switch real ID assigned by SAI SDK.
             */
            otai_object_id_t m_switch_rid;

            /**
             * @brief Map of default RIDs retrieved from Switch object.
             *
             * It will contain RIDs like CPU port, default virtual router, default
             * trap group. etc. Those objects here should be considered non
             * removable.
             */
            std::map<otai_attr_id_t,otai_object_id_t> m_default_rid_map;

            std::set<otai_object_id_t> m_coldBootDiscoveredVids;

            std::set<otai_object_id_t> m_warmBootDiscoveredVids;

            std::set<otai_object_id_t> m_warmBootNewDiscoveredVids;
    };
}
