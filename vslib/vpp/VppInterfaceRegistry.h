#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "VppInterface.h"

namespace saivs
{
    /*
     * Single owner of interface identity for the VPP SAI backend.
     *
     * Every field that is also a lookup key lives in one place, and every
     * mutation updates all affected indexes in the same step. That invariant is
     * the entire reason this class exists: the same facts used to be spread over
     * several side maps (m_lag_bond_map, m_swif_to_bdid, m_swif_to_port_id, and
     * the tap/name maps) whose lifetimes were only loosely related to each other,
     * so a teardown that missed one left the others answering with stale data.
     *
     * What deliberately stays outside: the SAI object hashes. The registry
     * knows names and oids, not attributes.
     *
     * The osif->hwif mapping parsed from sonic_vpp_ifmap.ini lives here too,
     * even though it is a seed read from a file rather than derived state,
     * because it is the only remaining way to answer "what is the VPP name of
     * this netdev" and splitting name resolution across two owners is what
     * this class exists to stop.
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
             * lookup in vppGetHwIfNameForPort() never touches a tap -- so
             * seeding one would make findByTap() answer for taps that do not
             * exist.
             */
            std::shared_ptr<VppPhysicalPort> addPhysicalPort(
                    _In_ const std::string& hwifName,
                    _In_ const std::string& osIf,
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

            /*
             * The VLAN oid is recorded so that a BVI can be resolved straight
             * from SAI_ROUTER_INTERFACE_ATTR_VLAN_ID without a SAI attribute
             * read to turn the oid back into a vlan id.
             */
            std::shared_ptr<VppVlanInterface> addBvi(
                    _In_ uint16_t vlanId,
                    _In_ sai_object_id_t vlanOid);

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
             * PORT, LAG or VLAN object id only; anything else is rejected.
             *
             * All three kinds supply their oid at registration, so this exists
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
            std::shared_ptr<VppInterface> findByOsIf(
                    _In_ const std::string& osIf) const;

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
             * Resolve a sw_if_index reported by VPP to the oid of the interface
             * it belongs to, walking from a sub-interface up to its parent.
             * That walk is the reason a sub-interface is a stored record rather
             * than a derived name.
             *
             * The oid is whatever the record carries: a PORT, a LAG or, for a
             * BVI, a VLAN. SAI_NULL_OBJECT_ID when the index is unknown or the
             * interface has no oid, as a tunnel or a bare sub-interface does
             * not.
             */
            sai_object_id_t resolveIfOid(
                    _In_ uint32_t swIfIndex) const;

            /*
             * Host netdev (linux-cp tap) of a PORT or LAG: "Ethernet0" for a
             * port, "be<N>" for a bond. Empty when the interface has no host
             * representation yet -- a port before its hostif is created, a bond
             * before vpp_ensure_lag_lcp() makes the be<N> pair. A miss means
             * "no host netdev", not "unknown interface".
             */
            std::string resolveTapName(
                    _In_ sai_object_id_t oid) const;

            /*
             * SONiC netdev of a PORT or LAG: "Ethernet0", "PortChannel<N>".
             *
             * Deliberately NOT resolveTapName(): a LAG has two host netdevs,
             * the teamd-owned PortChannel<N> returned here and the linux-cp tap
             * be<N> returned there. They coincide for a physical port. Use this
             * one for anything keyed on the SONiC name -- `ip link show`, VRF
             * enslavement, resolveHwIfByOsIf().
             */
            std::string resolveOsIf(
                    _In_ sai_object_id_t oid) const;

            /* Same, keyed on the VPP interface name. */
            std::string resolveOsIfByHwif(
                    _In_ const std::string& hwifName) const;

            /*
             * VPP interface name of a PORT, LAG or VLAN, with an optional
             * .<vlanId> sub-interface suffix: "TenGigabitEthernet0/0/0",
             * "BondEthernet1.100", "bvi1000". Empty on miss.
             *
             * A pure index lookup: it makes no SAI and no VPP call, so it
             * cannot register anything it does not already know about. The one
             * caller that has to cope with a port that has not been registered
             * yet is SwitchVpp::vppGetHwIfNameForPort().
             */
            std::string resolveHwIfName(
                    _In_ sai_object_id_t oid,
                    _In_ uint32_t vlanId) const;

            /*
             * A LAG that is fully realised: registered, of LAG type and bound
             * to a VPP sw_if_index. addLag() takes the bond id, the sw_if_index
             * and the oid in one call, so those three are either all present or
             * all absent. nullptr on miss.
             *
             * Callers reach the bond id and the LCP flag through asBond().
             */
            std::shared_ptr<VppInterface> findLag(
                    _In_ sai_object_id_t oid) const;

            /*
             * Bond ids currently in use.
             *
             * find_new_bond_id() picks the one kernel PortChannel that no LAG
             * has claimed yet, so it needs the set of already-allocated ids and
             * nothing else. Returning ids rather than exposing an iterator over
             * records keeps the indexes private.
             *
             * Note this cannot tell you the id OF a given PortChannel: a bond
             * record's SONiC name is built FROM its id, so the mapping only runs
             * that way. The kernel netdev name remains the sole source of a new
             * id.
             */
            std::unordered_set<uint32_t> collectBondIds() const;

            /*
             * VPP interface name of a host OS interface name ("Ethernet0",
             * "PortChannel1", "PortChannel1.100", "Vlan200"). The input is the
             * SONiC-side netdev name as produced by resolveOsIf(); a bond LCP
             * tap name ("be1") is deliberately not accepted. Empty on miss.
             *
             * Unlike every other resolver here this does NOT consult the
             * records: it answers from sonic_vpp_ifmap.ini plus the naming
             * rules, so it works before anything is registered. That is what
             * makes it usable as the registry's own seed --
             * getPortHwifNameFromLane() has to turn a port_config.ini name into
             * a hwif name before addPhysicalPort() can make a record.
             *
             * The flip side is that it answers by RULE, not by existence: ask
             * it about a PortChannel that was never created and it will still
             * say "BondEthernet<N>".
             */
            std::string resolveHwIfByOsIf(
                    _In_ const std::string& osIfName) const;

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

            /*
             * Parse sonic_vpp_ifmap.ini on first use. Retries on every call
             * until the file can actually be opened: at switch create time the
             * hwsku directory is not guaranteed to be there yet.
             */
            void loadIfMapping() const;

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
            std::map<std::string, std::shared_ptr<VppInterface>> m_byOsIf;

            /* only records that CURRENTLY have a host tap */
            std::map<std::string, std::shared_ptr<VppInterface>> m_byTap;

            /* PARTIAL: PORT and LAG oids only, never ROUTER_INTERFACE */
            std::map<sai_object_id_t, std::shared_ptr<VppInterface>> m_byOid;

            /* only records whose sw_if_index has been resolved */
            std::map<uint32_t, std::shared_ptr<VppInterface>> m_bySwIfIndex;

            /*
             * Physical ports only, straight out of sonic_vpp_ifmap.ini; every
             * logical interface is derived by rule instead. Mutable because it
             * is a read-through cache of an immutable file, which does not make
             * a lookup any less of an observer.
             */
            mutable std::map<std::string, std::string> m_osIfToHwIf;

            mutable bool m_ifMapLoaded = false;
    };
}
