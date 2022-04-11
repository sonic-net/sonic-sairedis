#pragma once

#include "syncd/SaiSwitchInterface.h"

#include <map>
#include <string>

namespace saiasiccmp
{
    class SaiSwitchAsic:
        public syncd::SaiSwitchInterface
    {
        private:

            SaiSwitchAsic(const SaiSwitchAsic&);
            SaiSwitchAsic& operator=(const SaiSwitchAsic&);

        public:

            SaiSwitchAsic(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t switchRid,
                    _In_ const std::map<sai_object_id_t, sai_object_id_t> vid2rid,
                    _In_ const std::map<sai_object_id_t, sai_object_id_t> rid2vid,
                    _In_ const std::map<std::string, sai_object_id_t>& hidden,
                    _In_ const std::map<sai_object_id_t, sai_object_type_t>& coldVids);

            virtual ~SaiSwitchAsic() = default;

        public:

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const override;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const override;

            virtual bool isDiscoveredRid(
                    _In_ sai_object_id_t rid) const override;

            virtual bool isColdBootDiscoveredRid(
                    _In_ sai_object_id_t rid) const override;

            virtual bool isSwitchObjectDefaultRid(
                    _In_ sai_object_id_t rid) const override;

            virtual bool isNonRemovableRid(
                    _In_ sai_object_id_t rid) const override;

            virtual std::set<sai_object_id_t> getDiscoveredRids() const override;

            virtual void removeExistingObject(
                    _In_ sai_object_id_t rid) override;

            virtual void removeExistingObjectReference(
                    _In_ sai_object_id_t rid) override;

            virtual void getDefaultMacAddress(
                    _Out_ sai_mac_t& mac) const override;

            virtual sai_object_id_t getDefaultValueForOidAttr(
                    _In_ sai_object_id_t rid,
                    _In_ sai_attr_id_t attr_id) override;

            virtual std::set<sai_object_id_t> getColdBootDiscoveredVids() const override;

            virtual std::set<sai_object_id_t> getWarmBootDiscoveredVids() const override;

            virtual void onPostPortCreate(
                    _In_ sai_object_id_t port_rid,
                    _In_ sai_object_id_t port_vid) override;

            virtual void postPortRemove(
                    _In_ sai_object_id_t portRid) override;

            virtual void collectPortRelatedObjects(
                    _In_ sai_object_id_t portRid) override;

        private:

            std::map<sai_object_id_t, sai_object_id_t> m_vid2rid;
            std::map<sai_object_id_t, sai_object_id_t> m_rid2vid;

    };
}
