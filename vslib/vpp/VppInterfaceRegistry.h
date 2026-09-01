#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "VppInterface.h"

namespace saivs
{
    /*
     * Single owner of interface identity for the VPP SAI backend.
     *
     * Every field that is also a lookup key lives in one place, and every
     * mutation updates all affected indexes in the same step. That invariant is
     * the entire reason this class exists: today the same facts are spread over
     * seven maps (m_osif_to_hwif_map, m_hwif_to_osif_map, m_ifname_to_port_id_map,
     * m_port_id_to_tapname, m_lag_bond_map, m_swif_to_bdid, m_swif_to_port_id)
     * whose lifetimes are only loosely related to each other.
     *
     * Threading: this class takes NO lock of its own. It lives under the
     * existing recursive m_apimutex held by the main/command thread and by the
     * FDB aging thread. The VPP API RX thread (staticMacEventCb) must NEVER
     * touch it -- that thread only enqueues onto m_mac_event_queue.
     *
     * This class makes no VPP API calls. Resolving a sw_if_index from VPP stays
     * in SwitchVpp, which then calls bindSwIfIndex(); that keeps the registry
     * free of SaiVppXlate.h and unit-testable on its own.
     */
    class VppInterfaceRegistry
    {
        public:

            VppInterfaceRegistry() = default;

            ~VppInterfaceRegistry() = default;

            VppInterfaceRegistry(const VppInterfaceRegistry&) = delete;
            VppInterfaceRegistry& operator=(const VppInterfaceRegistry&) = delete;

        public:

            // ---- creation -------------------------------------------------

            /*
             * Registered once the port's lane list is known, which is the point
             * at which it first HAS an identity: create_ports() creates ports
             * with no attributes at all and only then sets
             * SAI_PORT_ATTR_HW_LANE_LIST, so a port is anonymous until that set
             * lands. The lane set yields the SONiC name, sonic_vpp_ifmap.ini
             * turns that into the hwif name, and the oid is already in hand --
             * so unlike the old two step tap dance, a physical port is fully
             * identified in one call.
             *
             * The ifmap lookup is the CALLER'S job. It is platform config read
             * from a file, and keeping that out of here leaves the registry a
             * pure in-memory index: no I/O, no lazy state, no mutable members,
             * and unit-testable without fixture files.
             *
             * NO tap name is taken. A physical port exists and is routable
             * whether or not SAI ever creates a hostif for it -- the lane based
             * lookup in vpp_get_hwif_name() never touches a tap -- so seeding
             * one would make findByTap() answer for taps that do not exist.
             */
            std::shared_ptr<VppPhysicalPort> addPhysicalPort(
                    _In_ const std::string& hwifName,
                    _In_ const std::string& sonicName,
                    _In_ sai_object_id_t oid);

            /*
             * A bond is the one kind of interface whose whole identity is known
             * at once: vpp_create_lag() is handed the LAG oid, allocates the
             * bond id, and gets the sw_if_index back from
             * create_bond_interface() before it returns. Taking all three here
             * makes "a registered LAG always has an oid and an index" true by
             * construction, instead of leaving a window where it does not.
             *
             * The tap ("be<N>") is NOT taken: it only exists once the LCP pair
             * is created, which is a later and separate step.
             */
            std::shared_ptr<VppBondInterface> addLag(
                    _In_ uint32_t bondId,
                    _In_ uint32_t swIfIndex,
                    _In_ sai_object_id_t oid);

            /*
             * Idempotent: an existing sub-interface under the same name is
             * returned as is. The tagged VLAN member and sub-port RIF paths both
             * land here, but SONiC never overlaps them -- one is deleted before
             * the other is created -- so there is no ownership to arbitrate.
             */
            std::shared_ptr<VppSubInterface> addSubInterface(
                    _In_ const std::shared_ptr<VppInterface>& parent,
                    _In_ uint32_t subId,
                    _In_ uint16_t vlanId);

            std::shared_ptr<VppVlanInterface> addBvi(
                    _In_ uint16_t vlanId);

            std::shared_ptr<VppTunnelInterface> addTunnel(
                    _In_ const std::string& hwifName,
                    _In_ uint32_t vni);

        public:

            // ---- mutation, keyed by the primary key -----------------------

            /*
             * Late binding for records whose index only becomes known after a
             * sw_interface_dump. If another record already holds this index --
             * VPP recycles indexes after a delete -- the stale holder is
             * unbound first so the by_swif index can never alias.
             */
            bool bindSwIfIndex(
                    _In_ const std::string& hwifName,
                    _In_ uint32_t swIfIndex);

            bool unbindSwIfIndex(
                    _In_ const std::string& hwifName);

            /*
             * Record that a host netdev now exists for this interface, and its
             * name. Driven by hostif create for a physical port, by LCP pair
             * creation for a bond or a sub-port RIF.
             *
             * Kept apart from registration on purpose: tap lifetime is strictly
             * shorter than interface lifetime, and conflating the two is what
             * made the old m_port_id_to_tapname / m_hwif_to_osif_map pair
             * disagree with reality.
             */
            bool setTapName(
                    _In_ const std::string& hwifName,
                    _In_ const std::string& tapName);

            bool clearTapName(
                    _In_ const std::string& hwifName);

            /*
             * PORT or LAG object id only; anything else is rejected.
             *
             * Both kinds now supply their oid at registration, so this exists
             * for the rare re-bind rather than as a required second step.
             */
            bool setOid(
                    _In_ const std::string& hwifName,
                    _In_ sai_object_id_t oid);

            bool setRifOid(
                    _In_ const std::string& hwifName,
                    _In_ sai_object_id_t rifOid);

            bool setBdId(
                    _In_ const std::string& hwifName,
                    _In_ uint32_t bdId);

            bool clearBdId(
                    _In_ const std::string& hwifName);

        public:

            // ---- removal --------------------------------------------------

            /*
             * Cascades to sub-interfaces, mirroring VPP, which deletes them
             * along with their parent. Returns the number of records removed.
             */
            size_t remove(
                    _In_ const std::string& hwifName);

            size_t removeByOid(
                    _In_ sai_object_id_t oid);

        public:

            // ---- lookup, nullptr on miss ----------------------------------

            std::shared_ptr<VppInterface> findByHwif(
                    _In_ const std::string& hwifName) const;

            /*
             * Resolve a SONiC facing name ("Ethernet0", "PortChannel1"). Always
             * available for a registered interface, so this is what the lane
             * based port lookup and any config driven path should use.
             */
            std::shared_ptr<VppInterface> findBySonicName(
                    _In_ const std::string& sonicName) const;

            /*
             * Resolve a host netdev name. Answers only for interfaces that
             * currently HAVE a tap, so a miss is meaningful: it means there is
             * no host representation, not that the interface is unknown.
             */
            std::shared_ptr<VppInterface> findByTap(
                    _In_ const std::string& tapName) const;

            std::shared_ptr<VppInterface> findByOid(
                    _In_ sai_object_id_t oid) const;

            std::shared_ptr<VppInterface> findBySwIfIndex(
                    _In_ uint32_t swIfIndex) const;

            std::shared_ptr<VppSubInterface> findSubIf(
                    _In_ const std::string& parentHwifName,
                    _In_ uint32_t subId) const;

            /*
             * Resolve a sw_if_index reported by VPP to the PORT/LAG oid that
             * SAI_BRIDGE_PORT_ATTR_PORT_ID would carry, walking from a
             * sub-interface up to its parent. This is what the FDB learn/move
             * path needs, and the reason a sub-interface is a stored record
             * rather than a derived name.
             */
            sai_object_id_t resolvePortOid(
                    _In_ uint32_t swIfIndex) const;

            size_t size() const
            {
                return m_byHwif.size();
            }

            void clear();

        private:

            void indexRecord(
                    _In_ const std::shared_ptr<VppInterface>& rec);

            void unindexRecord(
                    _In_ const std::shared_ptr<VppInterface>& rec);

            std::vector<std::string> collectChildren(
                    _In_ const std::shared_ptr<VppInterface>& parent) const;

        private:

            /*
             * All five indexes hold a shared_ptr rather than a raw pointer.
             * Consistency is then structural: a missed erase leaves a stale
             * entry, which is diagnosable, instead of a dangling pointer, which
             * is undefined behaviour on the FDB hot path.
             */

            /* owning, primary key */
            std::map<std::string, std::shared_ptr<VppInterface>> m_byHwif;

            /* SONiC facing name; populated at registration, never late bound */
            std::map<std::string, std::shared_ptr<VppInterface>> m_bySonicName;

            /* only records that CURRENTLY have a host tap */
            std::map<std::string, std::shared_ptr<VppInterface>> m_byTap;

            /* PARTIAL: PORT and LAG oids only, never ROUTER_INTERFACE */
            std::map<sai_object_id_t, std::shared_ptr<VppInterface>> m_byOid;

            /* only records whose sw_if_index has been resolved */
            std::map<uint32_t, std::shared_ptr<VppInterface>> m_bySwIfIndex;
    };
}
