#pragma once
#include "swss/logger.h"
#include <cstdint>
#include <memory>
#include <string>

extern "C" {
#include "sai.h"
}

/*
 * Passive identity records for every kind of interface the VPP SAI backend
 * knows about.
 *
 * These objects hold data only. They deliberately do NOT call any VPP API
 * (create_bond_interface, configure_lcp_interface, refresh_interfaces_list,
 * ...) so that this header stays free of SaiVppXlate.h and the records remain
 * unit-testable on their own. All behaviour stays in SwitchVpp; ownership and
 * index consistency stay in VppInterfaceRegistry.
 */

namespace saivs
{
    class VppInterfaceRegistry;

    class VppBondInterface;
    class VppSubInterface;
    class VppVlanInterface;

    enum class VppInterfaceType
    {
        UNKNOWN = 0,

        PHYSICAL_PORT,      // front panel port, named by sonic_vpp_ifmap.ini
        LAG,                // BondEthernet<N>
        SUB_INTERFACE,      // <parent>.<vlan>, no PORT/LAG oid of its own
        VLAN_BVI,           // bvi<N>
    };

    class VppInterface
    {
        public:

            /*
             * Enumerators rather than static const members so that they need no
             * out-of-line definition when odr-used (C++14).
             */
            enum : uint32_t
            {
                /*
                 * sw_if_index is late bound: a physical port only learns its
                 * index from a sw_interface_dump, well after the record is
                 * created.
                 */
                SWIF_INDEX_INVALID = ~0u,

                /*
                 * "not a member of any bridge domain". This must stay distinct
                 * from "no record exists": the FDB path uses the presence of a
                 * bd_id as the gate that discards MAC events for L3 interfaces.
                 */
                BD_ID_INVALID = ~0u,
            };

        public:

            virtual ~VppInterface() = default;

            VppInterface(const VppInterface&) = delete;
            VppInterface& operator=(const VppInterface&) = delete;

        public:

            VppInterfaceType getType() const
            {
                SWSS_LOG_ENTER();
                return m_type;
            }

            /*
             * VPP side name, e.g. "TenGigabitEthernet0/0/0", "BondEthernet1",
             * "BondEthernet1.100", "bvi200". This is the primary natural key:
             * it is the only identity every interface kind always has.
             */
            const std::string& getHwifName() const
            {
                SWSS_LOG_ENTER();
                return m_hwifName;
            }

            /*
             * SONiC side name, e.g. "Ethernet0", "PortChannel1", "Vlan200",
             * "PortChannel1.100".
             *
             * A pure naming fact, known as soon as the interface is known. It
             * is what the lane based port lookup resolves against
             * (getPortHwifNameFromLane -> port_config.ini name ->
             * sonic_vpp_ifmap.ini), which is why it must be available long
             * before, and independently of, any host interface.
             *
             * This is NOT evidence that a host netdev exists. See getTapName().
             */
            const std::string& getOsIf() const
            {
                SWSS_LOG_ENTER();
                return m_osIf;
            }

            bool hasOsIf() const
            {
                SWSS_LOG_ENTER();
                return !m_osIf.empty();
            }

            /*
             * Linux side name of an EXISTING host netdev, e.g. "Ethernet0",
             * "be1", "tap_Vlan200", "be1.100".
             *
             * Late bound and optional. A physical port is fully usable before
             * SAI ever creates a hostif for it, and some never get one, so this
             * must not be seeded at registration time: doing so would make the
             * punt paths believe in a tap that does not exist.
             *
             * Usually equal to getOsIf() but not always - a sub-port RIF on
             * a bond deliberately uses "be<id>.<vlan>" rather than
             * "PortChannel<id>.<vlan>", which would collide with the kernel
             * 8021q netdev owned by the Linux bond stack.
             */
            const std::string& getTapName() const
            {
                SWSS_LOG_ENTER();
                return m_tapName;
            }

            bool hasTapName() const
            {
                SWSS_LOG_ENTER();
                return !m_tapName.empty();
            }

            /*
             * SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_LAG or, for a BVI,
             * SAI_OBJECT_TYPE_VLAN. The first two are what the FDB path
             * resolves to and what SAI_BRIDGE_PORT_ATTR_PORT_ID holds; the
             * third lets a BVI be found from the vlan oid a VLAN router
             * interface carries. A sub-interface never has one.
             */
            sai_object_id_t getOid() const
            {
                SWSS_LOG_ENTER();
                return m_oid;
            }

            bool hasOid() const
            {
                SWSS_LOG_ENTER();
                return m_oid != SAI_NULL_OBJECT_ID;
            }

            /*
             * SAI_OBJECT_TYPE_ROUTER_INTERFACE, kept separate from getOid() on
             * purpose: a RIF's SAI_ROUTER_INTERFACE_ATTR_PORT_ID is validated as
             * PORT *or LAG*, so a bond carries one too. Denormalised cache of an
             * attribute that also lives in the SAI object store.
             */
            sai_object_id_t getRifOid() const
            {
                SWSS_LOG_ENTER();
                return m_rifOid;
            }

            bool hasRifOid() const
            {
                SWSS_LOG_ENTER();
                return m_rifOid != SAI_NULL_OBJECT_ID;
            }

            uint32_t getSwIfIndex() const
            {
                SWSS_LOG_ENTER();
                return m_swIfIndex;
            }

            bool hasSwIfIndex() const
            {
                SWSS_LOG_ENTER();
                return m_swIfIndex != SWIF_INDEX_INVALID;
            }

            uint32_t getBdId() const
            {
                SWSS_LOG_ENTER();
                return m_bdId;
            }

            bool hasBdId() const
            {
                SWSS_LOG_ENTER();
                return m_bdId != BD_ID_INVALID;
            }

        public:

            /*
             * Per-type name construction. Currently duplicated across the code
             * base ("bvi%u" is built in two places, "be"+id in two more, the
             * BondEthernet prefix in seven); these are the single source.
             */
            virtual std::string deriveHwifName() const = 0;

            virtual std::string deriveTapName() const
            {
                SWSS_LOG_ENTER();
                return std::string();
            }

        public:

            /*
             * Cheap downcasts: an enum test plus static_cast, returning nullptr
             * on a type mismatch. Deliberately NOT dynamic_cast, because these
             * run on the FDB learn/move/age path for every MAC event.
             */
            const VppBondInterface* asBond() const;
            const VppSubInterface* asSubIf() const;
            const VppVlanInterface* asVlan() const;

            /*
             * Non-const overload, for the one piece of bond state that is not
             * identity and so is not owned by the registry: lcp_created.
             */
            VppBondInterface* asBond();

        protected:

            /*
             * Note that no tap name is accepted here: it is late bound through
             * VppInterfaceRegistry::setTapName() when the hostif is actually
             * created.
             *
             * bdId is accepted because for a BVI it is not late bound at all:
             * it is a pure function of the vlan id, known before the interface
             * exists. Every other kind joins a bridge domain as a later,
             * separate act and so leaves it invalid here.
             */
            VppInterface(
                    _In_ VppInterfaceType type,
                    _In_ const std::string& hwifName,
                    _In_ const std::string& osIf,
                    _In_ uint32_t bdId = BD_ID_INVALID):
                m_type(type),
                m_hwifName(hwifName),
                m_osIf(osIf),
                m_oid(SAI_NULL_OBJECT_ID),
                m_rifOid(SAI_NULL_OBJECT_ID),
                m_swIfIndex(SWIF_INDEX_INVALID),
                m_bdId(bdId)
            {
                SWSS_LOG_ENTER();
            }

        private:

            /*
             * Only the registry may mutate identity, because every one of these
             * fields is also an index key and the indexes must be updated in the
             * same step. That invariant is the entire reason the registry exists.
             */
            friend class VppInterfaceRegistry;

            VppInterfaceType m_type;

            std::string m_hwifName;

            std::string m_osIf;

            std::string m_tapName;

            sai_object_id_t m_oid;

            sai_object_id_t m_rifOid;

            uint32_t m_swIfIndex;

            uint32_t m_bdId;
    };

    /*
     * Front panel port. Both names come from sonic_vpp_ifmap.ini and are known
     * at seed time, so there is nothing to derive. The sw_if_index, the PORT
     * oid and the host tap all arrive later and independently of each other.
     */
    class VppPhysicalPort:
        public VppInterface
    {
        public:

            VppPhysicalPort(
                    _In_ const std::string& hwifName,
                    _In_ const std::string& osIf):
                VppInterface(VppInterfaceType::PHYSICAL_PORT, hwifName, osIf)
            {
                SWSS_LOG_ENTER();
            }

            std::string deriveHwifName() const override
            {
                SWSS_LOG_ENTER();
                return getHwifName();
            }

            /*
             * The tap SAI would create for this port, which is the port name
             * itself. Says nothing about whether it exists yet.
             */
            std::string deriveTapName() const override
            {
                SWSS_LOG_ENTER();
                return getOsIf();
            }
    };

    class VppBondInterface:
        public VppInterface
    {
        public:

            VppBondInterface(
                    _In_ uint32_t bondId):
                VppInterface(VppInterfaceType::LAG, hwifNameFor(bondId), osIfFor(bondId)),
                m_bondId(bondId),
                m_lcpCreated(false)
            {
                // empty
            }

            uint32_t getBondId() const
            {
                SWSS_LOG_ENTER();
                return m_bondId;
            }

            bool isLcpCreated() const
            {
                SWSS_LOG_ENTER();
                return m_lcpCreated;
            }

            void setLcpCreated(
                    _In_ bool created)
            {
                SWSS_LOG_ENTER();
                m_lcpCreated = created;
            }

            std::string deriveHwifName() const override
            {
                SWSS_LOG_ENTER();
                return hwifNameFor(m_bondId);
            }

            std::string deriveTapName() const override
            {
                SWSS_LOG_ENTER();
                return tapNameFor(m_bondId);
            }

        public:

            // must match BONDETHERNET_PREFIX in SwitchVppUtils.h
            static std::string hwifNameFor(
                    _In_ uint32_t bondId)
            {
                SWSS_LOG_ENTER();
                return "BondEthernet" + std::to_string(bondId);
            }

            // must match PORTCHANNEL_PREFIX in SwitchVppUtils.h
            static std::string osIfFor(
                    _In_ uint32_t bondId)
            {
                SWSS_LOG_ENTER();
                return "PortChannel" + std::to_string(bondId);
            }

            /*
             * The LCP tap for the bond. Deliberately "be<N>" and not
             * "PortChannel<N>": the latter is the kernel bond netdev owned by
             * teamd, not something linux-cp may bind to.
             */
            static std::string tapNameFor(
                    _In_ uint32_t bondId)
            {
                SWSS_LOG_ENTER();
                return "be" + std::to_string(bondId);
            }

        private:

            uint32_t m_bondId;

            bool m_lcpCreated;
    };

    /*
     * "<parent hwif>.<sub id>".
     *
     * Two call sites create sub-interfaces, and they produce differently shaped
     * objects under the same VPP name:
     *
     *   tagged VLAN member  - L2 bridged, no tap, no rif oid, has bd_id
     *   sub-port RIF        - L3, has an LCP tap and a rif oid, no bd_id
     *
     * The two are mutually exclusive and SONiC never overlaps them: it deletes
     * one before creating the other. So the record carries no ownership token --
     * which of the two a live sub-interface is, is already visible from
     * hasBdId() versus hasRifOid().
     *
     * The parent is held by composition rather than inheritance: a sub-interface
     * on a bond and one on a physical port are the same kind of thing, so
     * deriving from VppPhysicalPort would force a VppBondSubInterface as well.
     */
    class VppSubInterface:
        public VppInterface
    {
        public:

            VppSubInterface(
                    _In_ const std::shared_ptr<VppInterface>& parent,
                    _In_ uint32_t subId,
                    _In_ uint16_t vlanId):
                VppInterface(VppInterfaceType::SUB_INTERFACE,
                        hwifNameFor(parent ? parent->getHwifName() : std::string(), subId),
                        osIfFor(parent ? parent->getOsIf() : std::string(), subId)),
                m_parent(parent),
                m_subId(subId),
                m_vlanId(vlanId)
            {
                // empty
            }

            /*
             * Weak, so that a sub-interface never keeps its parent alive. The
             * registry cascades removal from parent to children, mirroring VPP,
             * which deletes sub-interfaces when the parent goes away.
             */
            std::shared_ptr<VppInterface> getParent() const
            {
                SWSS_LOG_ENTER();
                return m_parent.lock();
            }

            uint32_t getSubId() const
            {
                SWSS_LOG_ENTER();
                return m_subId;
            }

            uint16_t getVlanId() const
            {
                SWSS_LOG_ENTER();
                return m_vlanId;
            }

            std::string deriveHwifName() const override
            {
                SWSS_LOG_ENTER();
                auto parent = m_parent.lock();

                return hwifNameFor(parent ? parent->getHwifName() : std::string(), m_subId);
            }

            /*
             * The LCP tap a sub-port RIF would create. It hangs off the
             * PARENT'S TAP, not off the parent's SONiC name, so for a bond this
             * is "be<id>.<vlan>" while getOsIf() is
             * "PortChannel<id>.<vlan>". Empty while the parent has no tap.
             */
            std::string deriveTapName() const override
            {
                SWSS_LOG_ENTER();
                auto parent = m_parent.lock();

                if (!parent || !parent->hasTapName())
                {
                    return std::string();
                }

                return hwifNameFor(parent->getTapName(), m_subId);
            }

        public:

            static std::string hwifNameFor(
                    _In_ const std::string& parentHwifName,
                    _In_ uint32_t subId)
            {
                SWSS_LOG_ENTER();
                return parentHwifName + "." + std::to_string(subId);
            }

            static std::string osIfFor(
                    _In_ const std::string& parentOsIf,
                    _In_ uint32_t subId)
            {
                SWSS_LOG_ENTER();
                if (parentOsIf.empty())
                {
                    return std::string();
                }

                return parentOsIf + "." + std::to_string(subId);
            }

        private:

            std::weak_ptr<VppInterface> m_parent;

            uint32_t m_subId;

            uint16_t m_vlanId;
    };

    /*
     * BVI for a .1q VLAN. Its oid is the SAI_OBJECT_TYPE_VLAN object rather
     * than a port, and its rif oid is the SAI_ROUTER_INTERFACE_TYPE_VLAN one.
     */
    class VppVlanInterface:
        public VppInterface
    {
        public:

            VppVlanInterface(
                    _In_ uint16_t vlanId):
                VppInterface(VppInterfaceType::VLAN_BVI, hwifNameFor(vlanId), osIfFor(vlanId),
                             bdIdFor(vlanId)),
                m_vlanId(vlanId)
            {
                // empty
            }

            uint16_t getVlanId() const
            {
                SWSS_LOG_ENTER();
                return m_vlanId;
            }

            std::string deriveHwifName() const override
            {
                SWSS_LOG_ENTER();
                return hwifNameFor(m_vlanId);
            }

            std::string deriveTapName() const override
            {
                SWSS_LOG_ENTER();
                return tapNameFor(m_vlanId);
            }

        public:

            static std::string hwifNameFor(
                    _In_ uint16_t vlanId)
            {
                SWSS_LOG_ENTER();
                return "bvi" + std::to_string(vlanId);
            }

            static std::string osIfFor(
                    _In_ uint16_t vlanId)
            {
                SWSS_LOG_ENTER();
                return "Vlan" + std::to_string(vlanId);
            }

            static std::string tapNameFor(
                    _In_ uint16_t vlanId)
            {
                SWSS_LOG_ENTER();
                return "tap_Vlan" + std::to_string(vlanId);
            }

            /*
             * Bridge domains 1-4095 are statically reserved for .1q VLANs by
             * vlan id, so a BVI's bd_id needs no allocation and no lookup. The
             * identity is asserted in several places already (see
             * SwitchVppFdb.cpp "bd_id is same as VLAN ID for .1Q bridge"); this
             * is the single source for it.
             */
            static uint32_t bdIdFor(
                    _In_ uint16_t vlanId)
            {
                SWSS_LOG_ENTER();
                return vlanId;
            }

        private:

            uint16_t m_vlanId;
    };

    inline const VppBondInterface* VppInterface::asBond() const
    {
        SWSS_LOG_ENTER();

        if (m_type != VppInterfaceType::LAG)
            return nullptr;

        return static_cast<const VppBondInterface*>(this);
    }

    inline VppBondInterface* VppInterface::asBond()
    {
        SWSS_LOG_ENTER();

        if (m_type != VppInterfaceType::LAG)
            return nullptr;

        return static_cast<VppBondInterface*>(this);
    }

    inline const VppSubInterface* VppInterface::asSubIf() const
    {
        SWSS_LOG_ENTER();

        if (m_type != VppInterfaceType::SUB_INTERFACE)
            return nullptr;

        return static_cast<const VppSubInterface*>(this);
    }

    inline const VppVlanInterface* VppInterface::asVlan() const
    {
        SWSS_LOG_ENTER();

        if (m_type != VppInterfaceType::VLAN_BVI)
            return nullptr;

        return static_cast<const VppVlanInterface*>(this);
    }
}
