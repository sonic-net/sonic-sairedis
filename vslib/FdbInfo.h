#pragma once

extern "C" {
#include "sai.h"
}

#include <string>

namespace saivs
{
    /**
     * @brief FDB info class.
     *
     * Holds information about FDB arrived to switch.
     *
     * XXX: If performance will suffer, we can get rid of get and set and use
     * direct member access.  Also this can be converted to struct instead of
     * virtual class.
     */
    class FdbInfo
    {
        public:

            FdbInfo();

            virtual ~FdbInfo() = default;

        public: // gets

            sai_object_id_t getPortId() const;

            sai_vlan_id_t getVlanId() const;

            sai_object_id_t getBridgePortId() const;

            const sai_fdb_entry_t& getFdbEntry() const;

            uint32_t getTimestamp() const;

        public: // sets

            void setFdbEntry(
                    _In_ const sai_fdb_entry_t& entry);

            void setVlanId(
                    _In_ sai_vlan_id_t vlanId);

            void setPortId(
                    _In_ const sai_object_id_t portId);

            void setBridgePortId(
                    _In_ const sai_object_id_t portId);

            void setTimestamp(
                    _In_ uint32_t timestamp);

        public: // serialize

            std::string serialize() const;

            static FdbInfo deserialize(
                    _In_ const std::string& data);

        public: // operators

            bool operator<(
                    _In_ const FdbInfo& other) const;

            bool operator()(
                    _In_ const FdbInfo& lhs,
                    _In_ const FdbInfo& rhs) const;

        public: // TODO make private

            sai_object_id_t m_portId;

            sai_vlan_id_t m_vlanId;

            sai_object_id_t m_bridgePortId;

            sai_fdb_entry_t m_fdbEntry;

            uint32_t m_timestamp;
    };
}
