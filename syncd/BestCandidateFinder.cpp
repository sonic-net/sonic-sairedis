#include "BestCandidateFinder.h"
#include "VidManager.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

#include <inttypes.h>

#include <algorithm>

using namespace syncd;

BestCandidateFinder::BestCandidateFinder(
        _In_ const AsicView& currentView,
        _In_ const AsicView& temporaryView,
        _In_ std::shared_ptr<const SaiSwitchInterface> sw):
    m_currentView(currentView),
    m_temporaryView(temporaryView),
    m_switch(sw)
{
    SWSS_LOG_ENTER();

    // empty
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForLag(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For lag, let's try find matching LAG member which will be using the same
     * port, since we expect that port object can belong only to 1 lag.
     */

    /*
     * Find not processed LAG members, in both views, since lag member contains
     * LAG and PORT, then it should not be processed before LAG itself. But
     * since PORT objects on LAG members should be matched at the beginning of
     * comparison logic, then we can find batching LAG members based on the
     * same VID, since it will be the same in both views.
     */

    const auto tmpLagMembersNP = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

    /*
     * First we need to find at least 1 LAG member that belongs to temporary
     * object so we could extract port object.
     */

    sai_object_id_t tmpLagVid = temporaryObj->getVid();

    sai_object_id_t temporaryLagMemberPortVid = SAI_NULL_OBJECT_ID;

    for (const auto &lagMember: tmpLagMembersNP)
    {
        // lag member attr lag should always exists on
        const auto lagMemberLagAttr = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID);

        if (lagMemberLagAttr->getSaiAttr()->value.oid == tmpLagVid)
        {
            SWSS_LOG_NOTICE("found temp LAG member %s which uses temp LAG %s",
                    temporaryObj->m_str_object_id.c_str(),
                    lagMember->m_str_object_id.c_str());

            temporaryLagMemberPortVid = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID)->getSaiAttr()->value.oid;

            break;
        }
    }

    if (temporaryLagMemberPortVid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_NOTICE("failed to find temporary LAG member for LAG %s", sai_serialize_object_id(tmpLagVid).c_str());

        return nullptr;
    }

    /*
     * Now since we have port VID which should be the same in both current and
     * temporary view, let's find LAG member object that uses this port on
     * current view.
     */

    const auto curLagMembersNP = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

    for (const auto &lagMember: curLagMembersNP)
    {
        // lag member attr lag should always exists on
        const auto lagMemberPortAttr = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

        if (lagMemberPortAttr->getSaiAttr()->value.oid == temporaryLagMemberPortVid)
        {
            SWSS_LOG_NOTICE("found current LAG member %s which uses PORT %s",
                    lagMember->m_str_object_id.c_str(),
                    sai_serialize_object_id(temporaryLagMemberPortVid).c_str());

            /*
             * We found LAG member which uses the same PORT VID, let's extract
             * LAG and check if this LAG is on the candidate list.
             */

            sai_object_id_t currentLagVid = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID)->getSaiAttr()->value.oid;

            for (auto &c: candidateObjects)
            {
                if (c.obj->getVid() == currentLagVid)
                {
                    SWSS_LOG_NOTICE("found best candidate for temp LAG %s which is current LAG %s using PORT %s",
                            temporaryObj->m_str_object_id.c_str(),
                            sai_serialize_object_id(currentLagVid).c_str(),
                            sai_serialize_object_id(temporaryLagMemberPortVid).c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for LAG using LAG member and port");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForNextHopGroup(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For next hop group, let's try find matching NHG which will be based on
     * NHG set as SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID in route_entry.  We assume
     * that each class IPv4 and IPv6 will have different NHG, and each IP
     * prefix will only be assigned to one NHG.
     */

    /*
     * First find route entries on which temporary NHG is assigned.
     */

    const auto tmpRouteEntries = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    std::shared_ptr<SaiObj> tmpRouteCandidate = nullptr;

    for (auto tmpRoute: tmpRouteEntries)
    {
        if (!tmpRoute->hasAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID))
            continue;

        const auto routeNH = tmpRoute->getSaiAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID);

        if (routeNH->getSaiAttr()->value.oid != temporaryObj->getVid())
            continue;

        tmpRouteCandidate = tmpRoute;

        SWSS_LOG_NOTICE("Found route candidate for NHG: %s: %s",
                temporaryObj->m_str_object_id.c_str(),
                tmpRoute->m_str_object_id.c_str());

        break;
    }

    if (tmpRouteCandidate == nullptr)
    {
        SWSS_LOG_NOTICE("failed to find route candidate for NHG: %s",
                temporaryObj->m_str_object_id.c_str());

        return nullptr;
    }

    /*
     * We found route candidate, then let's find the same route on the candidate side.
     * But we will only compare prefix value.
     */

    const auto curRouteEntries = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    for (auto curRoute: curRouteEntries)
    {
        std::string tmpPrefix = sai_serialize_ip_prefix(tmpRouteCandidate->m_meta_key.objectkey.key.route_entry.destination);
        std::string curPrefix = sai_serialize_ip_prefix(curRoute->m_meta_key.objectkey.key.route_entry.destination);

        if (tmpPrefix != curPrefix)
            continue;

        /*
         * Prefixes are equal, now find candidate NHG.
         */

        if (!curRoute->hasAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID))
            continue;

        const auto routeNH = curRoute->getSaiAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID);

        sai_object_id_t curNextHopId = routeNH->getSaiAttr()->value.oid;

        for (auto candidate: candidateObjects)
        {
            if (curNextHopId != candidate.obj->getVid())
                continue;

            SWSS_LOG_NOTICE("Found NHG candidate %s", candidate.obj->m_str_object_id.c_str());

            return candidate.obj;
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for NEXT_HOP_GROUP using route_entry");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForAclCounter(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For acl counter we use SAI_ACL_COUNTER_ATTR_TABLE_ID to match exact
     * counter since if set, then table id will be matched previously.
     */

    std::vector<std::shared_ptr<SaiObj>> objs;

    const auto tmpAclTables = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE);

    for (auto& tmpAclTable: tmpAclTables)
    {
        auto tmpAclCounterTableIdAttr = temporaryObj->tryGetSaiAttr(SAI_ACL_COUNTER_ATTR_TABLE_ID);

        if (tmpAclCounterTableIdAttr == nullptr)
            continue;

        if (tmpAclCounterTableIdAttr->getOid() == SAI_NULL_OBJECT_ID)
            continue;

        if (tmpAclTable->getVid() != tmpAclCounterTableIdAttr->getOid())
            continue; // not this table

        if (tmpAclTable->getObjectStatus() != SAI_OBJECT_STATUS_FINAL)
            continue; // not processed

        sai_object_id_t aclTableRid = m_temporaryView.m_vidToRid.at(tmpAclTable->getVid());

        sai_object_id_t curAclTableVid = m_currentView.m_ridToVid.at(aclTableRid);

        for (auto c: candidateObjects)
        {
            auto curAclCounterTableIdAttr = c.obj->tryGetSaiAttr(SAI_ACL_COUNTER_ATTR_TABLE_ID);

            if (curAclCounterTableIdAttr == nullptr)
                continue;

            if (curAclCounterTableIdAttr->getOid() != curAclTableVid)
                continue;

            objs.push_back(c.obj);
            continue;
        }
    }

    if (objs.size() > 1)
    {
        // in this case more than 1 acl counters has the same acl table associated,
        // try to find best acl counter matching same acl entry field

        SWSS_LOG_INFO("more than 1 (%zu) best match on acl counter using acl table", objs.size());

        const auto tmpAclEntries = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_ENTRY);

        for (auto& tmpAclEntry: tmpAclEntries)
        {
            auto tmpAclEntryActionCounterAttr = tmpAclEntry->tryGetSaiAttr(SAI_ACL_ENTRY_ATTR_ACTION_COUNTER);

            if (tmpAclEntryActionCounterAttr == nullptr)
                continue; // skip acl entries with no counter

            if (tmpAclEntryActionCounterAttr->getOid() != temporaryObj->getVid())
                continue; // not the counter we are looking for

            for (auto&attr: tmpAclEntry->getAllAttributes())
            {
                auto*meta = attr.second->getAttrMetadata();

                if (!meta->isaclfield)
                    continue; // looking only for acl fields

                if (meta->isoidattribute)
                    continue; // only non oid fields

                auto tmpValue = attr.second->getStrAttrValue();

                const auto curAclEntries = m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_ENTRY);

                for (auto& curAclEntry: curAclEntries)
                {
                    auto curAclEntryAclFieldAttr = curAclEntry->tryGetSaiAttr(meta->attrid);

                    if (curAclEntryAclFieldAttr == nullptr)
                        continue; // this field is missing from current view

                    if (curAclEntryAclFieldAttr->getStrAttrValue() != tmpValue)
                        continue; // values are different, keep looking

                    auto curAclEntryActionCounterAttr = curAclEntry->tryGetSaiAttr(SAI_ACL_ENTRY_ATTR_ACTION_COUNTER);

                    if (curAclEntryActionCounterAttr == nullptr)
                        continue; // no counter

                    auto curAclCounter = m_currentView.m_oOids.at(curAclEntryActionCounterAttr->getOid());

                    if (curAclCounter->getObjectStatus() != SAI_OBJECT_STATUS_NOT_PROCESSED)
                        continue;

                    for (auto c: candidateObjects)
                    {
                        if (c.obj->getVid() == curAclCounter->getVid())
                        {
                            SWSS_LOG_NOTICE("found best ACL counter match based on ACL entry field: %s, %s",
                                    c.obj->m_str_object_id.c_str(),
                                    meta->attridname);
                            return c.obj;
                        }
                    }
                }
            }
        }

    }

    if (objs.size())
    {
        SWSS_LOG_NOTICE("found best ACL counter match based on ACL table: %s", objs.at(0)->m_str_object_id.c_str());

        return objs.at(0);
    }

    SWSS_LOG_NOTICE("failed to find best candidate for ACL_COUNTER using ACL table");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForAclTableGroup(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For acl table group let's try find matching INGRESS_ACL on matched PORT.
     */

    /*
     * Since we know that ports are matched, they have same VID/RID in both
     * temporary and current view.
     */

    const auto ports = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

    std::vector<int> aclPortAttrs = {
        SAI_PORT_ATTR_INGRESS_ACL,
        SAI_PORT_ATTR_EGRESS_ACL,
    };

    for (auto attrId: aclPortAttrs)
    {
        for (auto port: ports)
        {
            auto portAcl = port->tryGetSaiAttr(attrId);

            if (portAcl == nullptr)
                continue;

            if (portAcl ->getSaiAttr()->value.oid != temporaryObj->getVid())
                continue;

            SWSS_LOG_DEBUG("found port candidate %s for ACL table group",
                    port->m_str_object_id.c_str());

            auto curPort = m_currentView.m_oOids.at(port->getVid());

            portAcl = curPort->tryGetSaiAttr(attrId);

            if (portAcl == nullptr)
                continue;

            sai_object_id_t atgVid = portAcl->getSaiAttr()->value.oid;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() == atgVid)
                {
                    SWSS_LOG_INFO("found ALC table group candidate %s using port %s",
                            c.obj->m_str_object_id.c_str(),
                            port->m_str_object_id.c_str());

                    return c.obj;
                }
            }
        }
    }

    /*
     * Port didn't work, try to find match by LAG, but lag will be tricky,
     * since it will be not matched since if this unprocessed acl table group
     * is processed right now, then if it's assigned to lag then by design we
     * go recursively be attributes to match attributes first.
     */

    // TODO this could be helper method, since we will need this for router interface

    const auto tmpLags = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_LAG);

    for (auto tmpLag: tmpLags)
    {
        if (!tmpLag->hasAttr(SAI_LAG_ATTR_INGRESS_ACL))
            continue;

        auto inACL = tmpLag->getSaiAttr(SAI_LAG_ATTR_INGRESS_ACL);

        if (inACL->getSaiAttr()->value.oid != temporaryObj->getVid())
            continue;

        /*
         * We found LAG on which this ACL is present, but this object status is
         * not processed so we need to trace back to port using LAG member.
         */

        SWSS_LOG_INFO("found LAG candidate: lag status %d", tmpLag->getObjectStatus());

        const auto tmpLagMembers = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

        for (auto tmpLagMember: tmpLagMembers)
        {
            const auto tmpLagMemberLagAttr = tmpLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID);

            if (tmpLagMemberLagAttr->getSaiAttr()->value.oid != tmpLag->getVid())
                continue;

            const auto tmpLagMemberPortAttr = tmpLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

            sai_object_id_t tmpPortVid = tmpLagMemberPortAttr->getSaiAttr()->value.oid;

            SWSS_LOG_INFO("found tmp LAG member port! %s", sai_serialize_object_id(tmpPortVid).c_str());

            sai_object_id_t portRid = m_temporaryView.m_vidToRid.at(tmpPortVid);

            sai_object_id_t curPortVid = m_currentView.m_ridToVid.at(portRid);

            const auto curLagMembers = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

            for (auto curLagMember: curLagMembers)
            {
                const auto curLagMemberPortAttr = curLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

                if (curLagMemberPortAttr->getSaiAttr()->value.oid != curPortVid)
                    continue;

                const auto curLagMemberLagAttr = curLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID);

                sai_object_id_t curLagId = curLagMemberLagAttr->getSaiAttr()->value.oid;

                SWSS_LOG_INFO("found current LAG member: %s", curLagMember->m_str_object_id.c_str());

                auto curLag = m_currentView.m_oOids.at(curLagId);

                if (!curLag->hasAttr(SAI_LAG_ATTR_INGRESS_ACL))
                    continue;

                inACL = curLag->getSaiAttr(SAI_LAG_ATTR_INGRESS_ACL);

                for (auto c: candidateObjects)
                {
                    if (c.obj->getVid() != inACL->getSaiAttr()->value.oid)
                        continue;

                    SWSS_LOG_INFO("found best ACL table group match based on LAG ingress acl: %s", c.obj->m_str_object_id.c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for ACL_TABLE_GROUP using port");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForAclTable(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For acl table we can go to acl table group member and then to acl table group and port.
     */

    const auto tmpAclTableGroupMembers = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER);

    for (auto tmpAclTableGroupMember: tmpAclTableGroupMembers)
    {
        auto tmpAclTableId = tmpAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID);

        if (tmpAclTableId->getOid() != temporaryObj->getVid())
        {
            // this is not the expected acl table group member
            continue;
        }

        auto tmpAclTableGroupId = tmpAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID);

        /*
         * We have acl table group id, search on which port it's set, not let's
         * find on which port it's set.
         */

        const auto tmpPorts = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

        for (auto tmpPort: tmpPorts)
        {
            if (!tmpPort->hasAttr(SAI_PORT_ATTR_INGRESS_ACL))
                continue;

            auto tmpInACL = tmpPort->getSaiAttr(SAI_PORT_ATTR_INGRESS_ACL);

            if (tmpInACL->getOid() != tmpAclTableGroupId->getOid())
                continue;

            auto curPort = m_currentView.m_oOids.at(tmpPort->getVid());

            if (!curPort->hasAttr(SAI_PORT_ATTR_INGRESS_ACL))
                continue;

            auto curInACL = curPort->getSaiAttr(SAI_PORT_ATTR_INGRESS_ACL);

            /*
             * We found current ingress acl, now let's find acl table group members
             * that use this acl table group.
             */

            const auto curAclTableGroupMembers = m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER);

            for (auto curAclTableGroupMember: curAclTableGroupMembers)
            {
                auto curAclTableGroupId = curAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID);

                if (curAclTableGroupId->getOid() != curInACL->getOid())
                {
                    // this member uses different acl table group
                    continue;
                }

                auto curAclTableId = curAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID);

                /*
                 * We found possible current acl table ID, let's see if it's on
                 * candidate list. Note, it could be possible that many
                 * different paths will lead multiple possible candidates.
                 */

                for (auto c: candidateObjects)
                {
                    if (c.obj->getVid() == curAclTableId->getOid())
                    {
                        SWSS_LOG_INFO("found ACL table candidate %s using port %s",
                                c.obj->m_str_object_id.c_str(),
                                tmpPort->m_str_object_id.c_str());

                        return c.obj;
                    }
                }
            }
        }
    }

    // try using pre match in this case

    const auto tmpMembers = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER);

    for (auto tmpAclTableGroupMember: tmpMembers)
    {
        auto tmpAclTableIdAttr = tmpAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID);

        if (tmpAclTableIdAttr->getOid() != temporaryObj->getVid())
            continue;

        auto tmpAclTableGroupIdAttr = tmpAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID);

        auto tmpAclTableGroup = m_temporaryView.m_oOids.at(tmpAclTableGroupIdAttr->getOid());

        auto it = m_temporaryView.m_preMatchMap.find(tmpAclTableGroup->getVid());

        if (it == m_temporaryView.m_preMatchMap.end())
            continue;

        auto curAclTableGroupMembers = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER);

        for (auto curAclTableGroupMember: curAclTableGroupMembers)
        {
            auto curAclTableGroupIdAttr = curAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID);

            if (curAclTableGroupIdAttr->getOid() != it->second)
                continue;

            // we got acl table group member current that uses same acl table group as temporary

            auto curAclTableIdAttr = curAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID);

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() == curAclTableIdAttr->getOid())
                {
                    SWSS_LOG_INFO("found ACL table candidate %s using pre match ACL TABLE GROUP %s",
                            c.obj->m_str_object_id.c_str(),
                            tmpAclTableGroup->m_str_object_id.c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for ACL_TABLE using port");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForRouterInterface(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For router interface which is LOOPBACK, we could trace them to TUNNEL
     * object on which they are used. It could be not obvious, when multiple
     * tunnels will be used.
     */

    const auto typeAttr = temporaryObj->getSaiAttr(SAI_ROUTER_INTERFACE_ATTR_TYPE);

    if (typeAttr->getSaiAttr()->value.s32 != SAI_ROUTER_INTERFACE_TYPE_LOOPBACK)
    {
        SWSS_LOG_WARN("RIF %s is not LOOPBACK", temporaryObj->m_str_object_id.c_str());

        return nullptr;
    }

    const auto tmpTunnels = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL);

    for (auto tmpTunnel: tmpTunnels)
    {
        /*
         * Try match tunnel by src encap IP address.
         */

        if (!tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP))
        {
            // not encap src attribute, skip
            continue;
        }

        const std::string tmpSrcIP = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP)->getStrAttrValue();

        const auto curTunnels = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL);

        for (auto curTunnel: curTunnels)
        {
            if (!tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP))
            {
                // not encap src attribute, skip
                continue;
            }

            const std::string curSrcIP = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP)->getStrAttrValue();

            if (curSrcIP != tmpSrcIP)
            {
                continue;
            }

            /*
             * At this point we have both tunnels which ip matches.
             */

            if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE) &&
                curTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE))
            {
                auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);
                auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);

                if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                {
                    for (auto c: candidateObjects)
                    {
                        if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                            continue;

                        SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL underlay interface %s", c.obj->m_str_object_id.c_str());

                        return c.obj;
                    }
                }
            }

            if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE) &&
                curTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE))
            {
                auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);
                auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);

                if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                {
                    for (auto c: candidateObjects)
                    {
                        if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                            continue;

                        SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL overlay interface %s", c.obj->m_str_object_id.c_str());

                        return c.obj;
                    }
                }
            }
        }
    }

    // try find tunnel by TUNNEL_TERM_TABLE_ENTRY using SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP

    for (auto tmpTunnel: tmpTunnels)
    {
        const auto curTunnels = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL);

        for (auto curTunnel: curTunnels)
        {
            const auto tmpTunnelTermTableEtnries = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY);

            for (auto tmpTunnelTermTableEntry: tmpTunnelTermTableEtnries)
            {
                auto tmpTunnelId = tmpTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID);

                if (tmpTunnelId == nullptr)
                    continue;

                auto tmpDstIp = tmpTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP);

                if (tmpDstIp == nullptr)
                    continue;

                if (tmpTunnelId->getOid() != tmpTunnel->getVid())   // not this tunnel
                    continue;

                const auto curTunnelTermTableEtnries = m_currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY);

                for (auto curTunnelTermTableEntry: curTunnelTermTableEtnries)
                {
                    auto curTunnelId = curTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID);

                    if (curTunnelId == nullptr)
                        continue;

                    auto curDstIp = curTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP);

                    if (curDstIp == nullptr)
                        continue;

                    if (curTunnelId->getOid() != curTunnel->getVid())   // not this tunnel
                        continue;

                    if (curDstIp->getStrAttrValue() != tmpDstIp->getStrAttrValue())
                        continue;

                    if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE) &&
                            curTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE))
                    {
                        auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);
                        auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);

                        if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                        {
                            for (auto c: candidateObjects)
                            {
                                if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                                    continue;

                                SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL underlay interface %s", c.obj->m_str_object_id.c_str());

                                return c.obj;
                            }
                        }
                    }

                    if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE) &&
                            curTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE))
                    {
                        auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);
                        auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);

                        if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                        {
                            for (auto c: candidateObjects)
                            {
                                if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                                    continue;

                                SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL overlay interface %s", c.obj->m_str_object_id.c_str());

                                return c.obj;
                            }
                        }
                    }
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for LOOPBACK ROUTER_INTERFACE using tunnel");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForPolicer(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For policer we can see on which hostif trap group is set, and on which
     * hostif trap.  Hostif trap have SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE attribute
     * which is KEY and there can be only one trap of that type. we can use
     * that to match hostif trap group and later on policer.
     *
     * NOTE: policer can be set on default hostif trap group. In this case we
     * are getting processed and not processed objects into account.
     */

    const auto tmpTrapGroups = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP);

    for (auto tmpTrapGroup: tmpTrapGroups)
    {
        auto tmpTrapGroupPolicerAttr = tmpTrapGroup->tryGetSaiAttr(SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER);

        if (tmpTrapGroupPolicerAttr == nullptr)
        {
            // no policer attribute
            continue;
        }

        if (tmpTrapGroupPolicerAttr->getOid() != temporaryObj->getVid())
        {
            // not this policer
            continue;
        }

        /*
         * Found hostif trap group which have this policer, now find hostif
         * trap type with this trap group.
         */

        const auto tmpTraps = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

        for (auto tmpTrap: tmpTraps)
        {
            auto tmpTrapGroupAttr = tmpTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

            if (tmpTrapGroupAttr == nullptr)
                continue;

            if (tmpTrapGroupAttr->getOid() != tmpTrapGroup->getVid())
                continue;

            auto tmpTrapTypeAttr = tmpTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

            SWSS_LOG_INFO("trap type: %s", tmpTrapTypeAttr->getStrAttrValue().c_str());

            /*
             * We have temporary trap type, let's find that trap in current view.
             */

            const auto curTraps = m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

            for (auto curTrap: curTraps)
            {
                auto curTrapTypeAttr = curTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

                if (curTrapTypeAttr->getStrAttrValue() != tmpTrapTypeAttr->getStrAttrValue())
                    continue;

                /*
                 * We have that trap, let's extract trap group.
                 */

                auto curTrapGroupAttr = curTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

                if (curTrapGroupAttr == nullptr)
                    continue;

                sai_object_id_t curTrapGroupVid = curTrapGroupAttr->getOid();

                /*
                 * If value is not set, it should point to SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP
                 */

                if (curTrapGroupVid == SAI_NULL_OBJECT_ID)
                    continue;

                auto curTrapGroup = m_currentView.m_oOids.at(curTrapGroupVid);

                auto curTrapGroupPolicerAttr = curTrapGroup->tryGetSaiAttr(SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER);

                if (curTrapGroupPolicerAttr == nullptr)
                {
                    // no policer attribute
                    continue;
                }

                for (auto c: candidateObjects)
                {
                    if (c.obj->getVid() != curTrapGroupPolicerAttr->getOid())
                        continue;

                    SWSS_LOG_INFO("found best POLICER based on hostif trap group %s", c.obj->m_str_object_id.c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for POLICER using hostif trap group");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForHostifTrapGroup(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For hostif trap group we can see on which hostif trap group is set.
     * Hostif trap have SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE attribute which is KEY
     * and there can be only one trap of that type. we can use that to match
     * hostif trap group.
     */

    const auto tmpTraps = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

    for (auto tmpTrap: tmpTraps)
    {
        auto tmpTrapGroupAttr = tmpTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

        if (tmpTrapGroupAttr == nullptr)
            continue;

        if (tmpTrapGroupAttr->getOid() != temporaryObj->getVid())
            continue;

        auto tmpTrapTypeAttr = tmpTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

        SWSS_LOG_INFO("trap type: %s", tmpTrapTypeAttr->getStrAttrValue().c_str());

        /*
         * We have temporary trap type, let's find that trap in current view.
         */

        const auto curTraps = m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

        for (auto curTrap: curTraps)
        {
            auto curTrapTypeAttr = curTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

            if (curTrapTypeAttr->getStrAttrValue() != tmpTrapTypeAttr->getStrAttrValue())
                continue;

            /*
             * We have that trap, let's extract trap group.
             */

            auto curTrapGroupAttr = curTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

            if (curTrapGroupAttr == nullptr)
                continue;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curTrapGroupAttr->getOid())
                    continue;

                SWSS_LOG_INFO("found best HOSTIF TRAP GROUP based on hostif trap %s", c.obj->m_str_object_id.c_str());

                return c.obj;
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for HOSTIF_TRAP_GROUP using hostif trap");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForBufferPool(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For buffer pool using buffer profile which could be set on ingress
     * priority group or queue. Those two should be already matched.
     */

    const auto tmpBufferProfiles = m_temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_BUFFER_PROFILE);

    for (auto tmpBufferProfile: tmpBufferProfiles)
    {
        auto tmpPoolIdAttr = tmpBufferProfile->tryGetSaiAttr(SAI_BUFFER_PROFILE_ATTR_POOL_ID);

        if (tmpPoolIdAttr == nullptr)
            continue;

        if (tmpPoolIdAttr->getOid() != temporaryObj->getVid())
            continue;

        /*
         * We have temporary buffer profile which uses this buffer pool, let's
         * find ingress priority group or queue on which it could be set.
         */

        auto tmpQueues = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_QUEUE);

        for (auto tmpQueue: tmpQueues)
        {
            auto tmpBufferProfileIdAttr = tmpQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID);

            if (tmpBufferProfileIdAttr == nullptr)
                continue;

            if (tmpBufferProfileIdAttr->getOid() != tmpBufferProfile->getVid())
                continue;

            if (tmpQueue->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
                continue;

            // we can use tmp VID since object is matched and both vids are the same
            auto curQueue = m_currentView.m_oOids.at(tmpQueue->getVid());

            auto curBufferProfileIdAttr = curQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID);

            if (curBufferProfileIdAttr == nullptr)
                continue;

            if (curBufferProfileIdAttr->getOid() == SAI_NULL_OBJECT_ID)
                continue;

            // we have buffer profile

            auto curBufferProfile = m_currentView.m_oOids.at(curBufferProfileIdAttr->getOid());

            auto curPoolIdAttr = curBufferProfile->tryGetSaiAttr(SAI_BUFFER_PROFILE_ATTR_POOL_ID);

            if (curPoolIdAttr == nullptr)
                continue;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curPoolIdAttr->getOid())
                    continue;

                SWSS_LOG_INFO("found best BUFFER POOL based on buffer profile and queue %s", c.obj->m_str_object_id.c_str());

                return c.obj;
            }
        }

        /*
         * Queues didn't worked, lets try to use ingress priority groups.
         */

        auto tmpIngressPriorityGroups = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP);

        for (auto tmpIngressPriorityGroup: tmpIngressPriorityGroups)
        {
            auto tmpBufferProfileIdAttr = tmpIngressPriorityGroup->tryGetSaiAttr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

            if (tmpBufferProfileIdAttr == nullptr)
                continue;

            if (tmpBufferProfileIdAttr->getOid() != tmpBufferProfile->getVid())
                continue;

            if (tmpIngressPriorityGroup->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
                continue;

            // we can use tmp VID since object is matched and both vids are the same
            auto curIngressPriorityGroup = m_currentView.m_oOids.at(tmpIngressPriorityGroup->getVid());

            auto curBufferProfileIdAttr = curIngressPriorityGroup->tryGetSaiAttr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

            if (curBufferProfileIdAttr == nullptr)
                continue;

            if (curBufferProfileIdAttr->getOid() == SAI_NULL_OBJECT_ID)
                continue;

            // we have buffer profile

            auto curBufferProfile = m_currentView.m_oOids.at(curBufferProfileIdAttr->getOid());

            auto curPoolIdAttr = curBufferProfile->tryGetSaiAttr(SAI_BUFFER_PROFILE_ATTR_POOL_ID);

            if (curPoolIdAttr == nullptr)
                continue;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curPoolIdAttr->getOid())
                    continue;

                SWSS_LOG_INFO("found best BUFFER POOL based on buffer profile and ingress priority group %s", c.obj->m_str_object_id.c_str());

                return c.obj;
            }
        }

    }

    SWSS_LOG_NOTICE("failed to find best candidate for BUFFER_POOL using buffer profile, ipg and queue");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForBufferProfile(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For buffer profile we will try using SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE
     * or SAI_QUEUE_ATTR_BUFFER_PROFILE_ID for matching.
     *
     * If we are here, and buffer profile has assigned buffer pool, then buffer
     * pool was matched correctly or best effort. Then we have trouble matching
     * buffer profile since their configuration could be the same.  This search
     * here should solve the issue.
     */

    auto tmpQueues = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_QUEUE);

    for (auto tmpQueue: tmpQueues)
    {
        auto tmpBufferProfileIdAttr = tmpQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID);

        if (tmpBufferProfileIdAttr == nullptr)
            continue;

        if (tmpBufferProfileIdAttr->getOid() != temporaryObj->getVid())
            continue;

        if (tmpQueue->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
            continue;

        // we can use tmp VID since object is matched and both vids are the same
        auto curQueue = m_currentView.m_oOids.at(tmpQueue->getVid());

        auto curBufferProfileIdAttr = curQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID);

        if (curBufferProfileIdAttr == nullptr)
            continue;

        // we have buffer profile

        for (auto c: candidateObjects)
        {
            if (c.obj->getVid() != curBufferProfileIdAttr->getOid())
                continue;

            SWSS_LOG_INFO("found best BUFFER PROFILE based on queues %s", c.obj->m_str_object_id.c_str());

            return c.obj;
        }
    }

    auto tmpIPGs = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP);

    for (auto tmpIPG: tmpIPGs)
    {
        auto tmpBufferProfileIdAttr = tmpIPG->tryGetSaiAttr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

        if (tmpBufferProfileIdAttr == nullptr)
            continue;

        if (tmpBufferProfileIdAttr->getOid() != temporaryObj->getVid())
            continue;

        if (tmpIPG->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
            continue;

        // we can use tmp VID since object is matched and both vids are the same
        auto curIPG = m_currentView.m_oOids.at(tmpIPG->getVid());

        auto curBufferProfileIdAttr = curIPG->tryGetSaiAttr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

        if (curBufferProfileIdAttr == nullptr)
            continue;

        // we have buffer profile

        for (auto c: candidateObjects)
        {
            if (c.obj->getVid() != curBufferProfileIdAttr->getOid())
                continue;

            SWSS_LOG_INFO("found best BUFFER PROFILE based on IPG %s", c.obj->m_str_object_id.c_str());

            return c.obj;
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for BUFFER_PROFILE using queues and ipgs");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForTunnelMap(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For tunnel map, lets find tunnel map entry with unique
     * SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE and use it's value for matching.
     */

    auto tmpTunnelMapEntries = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);

    for (auto& tmpTunnelMapEntry: tmpTunnelMapEntries)
    {
        auto tmpTunnelMapAttr = tmpTunnelMapEntry->tryGetSaiAttr(SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP);

        if (tmpTunnelMapAttr == nullptr)
            continue;

        if (tmpTunnelMapAttr->getOid() != temporaryObj->getVid())
            continue;

        auto tmpVlanIdValueAttr = tmpTunnelMapEntry->tryGetSaiAttr(SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE);

        if (tmpVlanIdValueAttr == nullptr)
            continue;

        uint16_t vlanId = tmpVlanIdValueAttr->getSaiAttr()->value.u16;

        // now find map entry with same vlan id on current object list

        auto curTunnelMapEntries = m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);

        for (auto& curTunnelMapEntry: curTunnelMapEntries)
        {
            auto curVlanIdValueAttr = curTunnelMapEntry->tryGetSaiAttr(SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE);

            if (curVlanIdValueAttr == nullptr)
                continue;

            if (curVlanIdValueAttr->getSaiAttr()->value.u16 != vlanId)
                continue; // wrong vlan id, keep looking

            auto curTunnelMapAttr = curTunnelMapEntry->tryGetSaiAttr(SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP);

            if (curTunnelMapAttr == nullptr)
                continue;

            if (curTunnelMapAttr->getOid() == SAI_NULL_OBJECT_ID)
                continue;

            auto curTunnelMap = m_currentView.m_oOids.at(curTunnelMapAttr->getOid());

            if (curTunnelMap->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
                continue; // object already matched, we need to search more

            // we have current tunnel map, see if it's on candidate list

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curTunnelMap->getVid())
                    continue;

                SWSS_LOG_INFO("found best TUNNEL MAP based on tunnel map entry vlan id value %s", c.obj->m_str_object_id.c_str());

                return c.obj;
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for TUNNEL_MAP using tunnel map entry vlan id value");

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForWred(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For WRED we will first if it's assigned to any of the queues.
     */

    auto tmpQueues = m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_QUEUE);

    for (auto tmpQueue: tmpQueues)
    {
        auto tmpWredProfileIdAttr = tmpQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_WRED_PROFILE_ID);

        if (tmpWredProfileIdAttr == nullptr)
            continue; // no WRED attribute on queue

        if (tmpWredProfileIdAttr->getOid() != temporaryObj->getVid())
            continue; // not this queue

        if (tmpQueue->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
            continue; // we only look for matched queues

        // we found matched queue with this WRED

        // we can use tmp VID since object is matched and both vids are the same
        auto curQueue = m_currentView.m_oOids.at(tmpQueue->getVid());

        auto curWredProfileIdAttr = curQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_WRED_PROFILE_ID);

        if (curWredProfileIdAttr == nullptr)
            continue; // current queue has no WRED attribute

        if (curWredProfileIdAttr->getOid() == SAI_NULL_OBJECT_ID)
            continue; // WRED is NULL on current queue

        for (auto c: candidateObjects)
        {
            if (c.obj->getVid() != curWredProfileIdAttr->getOid())
                continue;

            SWSS_LOG_INFO("found best WRED based on queue %s", c.obj->m_str_object_id.c_str());

            return c.obj;
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for WRED using queue");

    return nullptr;
}


std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForGenericObjectUsingPreMatchMap(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    if (temporaryObj->isOidObject() == false)
        return std::shared_ptr<SaiObj>();

    auto it = m_temporaryView.m_preMatchMap.find(temporaryObj->getVid());

    if (it == m_temporaryView.m_preMatchMap.end()) // no luck, there was no vid in pre match
        return nullptr;

    for (auto c: candidateObjects)
    {
        if (c.obj->getVid() == it->second)
        {
            SWSS_LOG_INFO("found pre match vid %s (tmp) %s (cur)",
                    sai_serialize_object_id(it->first).c_str(),
                    sai_serialize_object_id(it->second).c_str());

            return c.obj;
        }
    }

    return nullptr;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForGenericObjectUsingLabel(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    switch (temporaryObj->getObjectType())
    {
        case SAI_OBJECT_TYPE_LAG:
            return findCurrentBestMatchForGenericObjectUsingLabel(temporaryObj, candidateObjects, SAI_LAG_ATTR_LABEL);

        case SAI_OBJECT_TYPE_VIRTUAL_ROUTER:
            return findCurrentBestMatchForGenericObjectUsingLabel(temporaryObj, candidateObjects, SAI_VIRTUAL_ROUTER_ATTR_LABEL);

        default:
            return nullptr;
    }
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForGenericObjectUsingLabel(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects,
        _In_ sai_attr_id_t attrId)
{
    SWSS_LOG_ENTER();

    auto labelAttr = temporaryObj->tryGetSaiAttr(attrId);

    if (!labelAttr)
    {
        // no label attribute on that object
        return nullptr;
    }

    auto label = labelAttr->getStrAttrValue();

    std::vector<sai_object_compare_info_t> sameLabel;

    for (auto& co: candidateObjects)
    {
        if (co.obj->hasAttr(attrId) && co.obj->getSaiAttr(attrId)->getStrAttrValue() == label)
        {
            sameLabel.push_back(co);
        }
    }

    if (sameLabel.size() == 0)
    {
        // no objects with that label, fallback to attr count
        return nullptr;
    }

    if (sameLabel.size() == 1)
    {
        SWSS_LOG_NOTICE("matched object by label '%s' for %s:%s",
            label.c_str(),
            temporaryObj->m_str_object_type.c_str(),
            temporaryObj->m_str_object_id.c_str());

        return sameLabel.at(0).obj;
    }

    SWSS_LOG_WARN("same label '%s' found on multiple objects for %s:%s, selecting one with most common atributes",
            label.c_str(),
            temporaryObj->m_str_object_type.c_str(),
            temporaryObj->m_str_object_id.c_str());

    std::sort(sameLabel.begin(), sameLabel.end(), compareByEqualAttributes);

    return sameLabel.at(0).obj;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForGenericObjectUsingGraph(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<SaiObj> candidate = nullptr;

    candidate = findCurrentBestMatchForGenericObjectUsingPreMatchMap(temporaryObj, candidateObjects);

    if (candidate != nullptr)
        return candidate;

    switch (temporaryObj->getObjectType())
    {
        case SAI_OBJECT_TYPE_LAG:
            candidate = findCurrentBestMatchForLag(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:
            candidate = findCurrentBestMatchForNextHopGroup(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ACL_TABLE_GROUP:
            candidate = findCurrentBestMatchForAclTableGroup(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ACL_COUNTER:
            candidate = findCurrentBestMatchForAclCounter(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ROUTER_INTERFACE:
            candidate = findCurrentBestMatchForRouterInterface(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_POLICER:
            candidate = findCurrentBestMatchForPolicer(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP:
            candidate = findCurrentBestMatchForHostifTrapGroup(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ACL_TABLE:
            candidate = findCurrentBestMatchForAclTable(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_BUFFER_POOL:
            candidate = findCurrentBestMatchForBufferPool(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_WRED:
            candidate = findCurrentBestMatchForWred(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_BUFFER_PROFILE:
            candidate = findCurrentBestMatchForBufferProfile(temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_TUNNEL_MAP:
            candidate = findCurrentBestMatchForTunnelMap(temporaryObj, candidateObjects);
            break;

        default:
            break;
    }

    return candidate;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForGenericObjectUsingHeuristic(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    auto candidate = findCurrentBestMatchForGenericObjectUsingGraph(temporaryObj, candidateObjects);

    if (candidate != nullptr)
    {
        return candidate;
    }

    /*
     * Idea is to count all dependencies that uses this object.  this may not
     * be a good approach since logic may choose wrong candidate.
     */

    int tempCount = findAllChildsInDependencyTreeCount(m_temporaryView, temporaryObj);

    SWSS_LOG_DEBUG("%s count usage: %d", temporaryObj->m_str_object_type.c_str(), tempCount);

    std::vector<int> counts;

    int exact = 0;
    std::shared_ptr<SaiObj> matched;

    for (auto &c: candidateObjects)
    {
        int count = findAllChildsInDependencyTreeCount(m_currentView, c.obj);

        SWSS_LOG_DEBUG("candidate count usage: %d", count);

        counts.push_back(count);

        if (count == tempCount)
        {
            exact++;
            matched = c.obj;
        }
    }

    if (exact == 1)
    {
        return matched;
    }

    SWSS_LOG_WARN("heuristic failed for %s, selecting at random (count: %d, exact match: %d)",
            temporaryObj->m_str_object_type.c_str(),
            tempCount,
            exact);

    return selectRandomCandidate(candidateObjects);
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForGenericObject(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * This method will try to find current best match object for a given
     * temporary object. This method should be used only on object id objects,
     * since non object id structures also contains object id which in this
     * case are not take into account. Besides for objects like FDB, ROUTE or
     * NEIGHBOR we can do quick hash lookup instead of looping for all objects
     * where there can be a lot of them.
     *
     * Special case here we can add later on is VLAN, since we can have a lot
     * of VLANS so instead looking via all of them we just need to make sure
     * that we will create reverse map via VLAN_ID KEY and then we can make
     * hash lookup to see if such vlan is present.
     */

    /*
     * Since our system design is to restart orchagent without restarting syncd
     * and recreating objects and reassign new VIDs created inside orchagent,
     * in our most cases values of objects will not change.  This will cause to
     * make our comparison logic here fairly simple:
     *
     * Find all objects that have the same equal attributes on current object
     * and choose the one with the most attributes that match current and
     * temporary object.
     *
     * This seems simple, but there are a lot of cases that needs to be taken
     * into account:
     *
     * - what if we have several objects with the same number of equal
     *   attributes then we can choose at random or implement some heuristic
     *   logic to try figure out which of those objects will be the best, even
     *   then if we choose wrong object, then there can be a lot of removes and
     *   recreating objects on the ASIC
     *
     * - what if in temporary object CREATE_ONLY attributes don't match but
     *   many of CREATE_AND_SET are the same, in that case we can choose object
     *   with most matching attributes, but object will still needs to be
     *   destroyed because we need to set new CREATE_ONLY attributes
     *
     * - there are also cases with default values of attributes where attribute
     *   is present only in one object but on other one it have default value
     *   and this default value is the same as the one in attribute
     *
     * - another case is for objects that needs to be removed since there are
     *   no corresponding objects in temporary view, but they can't be removed,
     *   objects like PORT or default QUEUEs or INGRESS_PRIORITY_GROUPs, then
     *   we need to bring their current set values to default ones, which also
     *   can be challenging since we need to know previous default value and it
     *   could be assigned by switch internally, like default MAC address or
     *   default TRAP group etc
     *
     * - there is also interesting case with KEYs attributes which when
     *   doing remove/create they needs to be removed first since
     *   we can't have 2 identical cases
     *
     * There are a lot of aspects to consider here, in here we will cake only
     * couple of them in consideration, and other will be taken care inside
     * processObjectForViewTransition method which will handle all other cases
     * not mentioned here.
     */

    /*
     * First check if object is oid object, if yes, check if it status is
     * matched.
     */

    if (!temporaryObj->isOidObject())
    {
        SWSS_LOG_THROW("non object id %s is used in generic method, please implement special case, FIXME",
                temporaryObj->m_str_object_type.c_str());
    }

    /*
     * Get not processed objects of temporary object type, and all attributes
     * that are set on that object. This function should be used only on oid
     * object ids, since for non object id finding best match is based on
     * struct entry of object id.
     */

    sai_object_type_t object_type = temporaryObj->getObjectType();

    const auto notProcessedObjects = m_currentView.getNotProcessedObjectsByObjectType(object_type);

    const auto attrs = temporaryObj->getAllAttributes();

    /*
     * Complexity here is O((n^2)*m) since we iterate via all not processed
     * objects, then we iterate through all present attributes.  N is squared
     * since for given object type we iterate via entire list for each object,
     * this can be optimized a little bit inside AsicView class.
     */

    SWSS_LOG_INFO("not processed objects for %s: %zu, attrs: %zu",
            temporaryObj->m_str_object_type.c_str(),
            notProcessedObjects.size(),
            attrs.size());

    std::vector<sai_object_compare_info_t> candidateObjects;

    for (const auto &currentObj: notProcessedObjects)
    {
        sai_object_compare_info_t soci = { 0, currentObj };

        bool has_different_create_only_attr = false;

        /*
         * NOTE: we only iterate by attributes that are present in temporary
         * view. It may happen that current view has some additional attributes
         * set that are create only and value can't be updated then, so in that
         * case such object must be disqualified from being candidate.
         */

        for (const auto &attr: attrs)
        {
            sai_attr_id_t attrId = attr.first;

            /*
             * Function hasEqualAttribute check if attribute exists on both objects.
             */

            if (hasEqualAttribute(m_currentView, m_temporaryView, currentObj, temporaryObj, attrId))
            {
                soci.equal_attributes++;

                SWSS_LOG_INFO("ob equal %s %s, %s: %s",
                        temporaryObj->m_str_object_id.c_str(),
                        currentObj->m_str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());
            }
            else
            {
                SWSS_LOG_INFO("ob not equal %s %s, %s: %s",
                        temporaryObj->m_str_object_id.c_str(),
                        currentObj->m_str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());

                /*
                 * Function hasEqualAttribute returns true only when both
                 * attributes are existing and both are equal, so here it
                 * returned false, so it may mean 2 things:
                 *
                 * - attribute doesn't exist in current view, or
                 * - attributes are different
                 *
                 * If we check if attribute also exists in current view and has
                 * CREATE_ONLY flag then attributes are different and we
                 * disqualify this object since new temporary object needs to
                 * pass new different attribute with CREATE_ONLY flag.
                 *
                 * Case when attribute doesn't exist is much more complicated
                 * since it maybe conditional and have default value, we will
                 * do that check when we select best match.
                 */

                /*
                 * Get attribute metadata to see if contains CREATE_ONLY flag.
                 */

                const sai_attr_metadata_t* meta = attr.second->getAttrMetadata();

                if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags) && currentObj->hasAttr(attrId))
                {
                    has_different_create_only_attr = true;

                    SWSS_LOG_INFO("obj has not equal create only attributes %s",
                            temporaryObj->m_str_object_id.c_str());

                    /*
                     * In this case there is no need to compare other
                     * attributes since we won't be able to update them anyway.
                     */

                    break;
                }

                if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags) && !currentObj->hasAttr(attrId))
                {
                    /*
                     * This attribute exists only on temporary view and it's
                     * create only.  If it has default value, check if it's the
                     * same as current.
                     */

                    auto curDefault = getSaiAttrFromDefaultValue(m_currentView, m_switch, *meta);

                    if (curDefault != nullptr)
                    {
                        if (curDefault->getStrAttrValue() != attr.second->getStrAttrValue())
                        {
                            has_different_create_only_attr = true;

                            SWSS_LOG_INFO("obj has not equal create only attributes %s (default): %s",
                                    temporaryObj->m_str_object_id.c_str(),
                                    meta->attridname);
                            break;
                        }
                        else
                        {
                            SWSS_LOG_INFO("obj has equal create only value %s (default): %s",
                                    temporaryObj->m_str_object_id.c_str(),
                                    meta->attridname);
                        }
                    }
                }
            }
        }

        /*
         * Before we add this object as candidate, see if there are some create
         * only attributes which are not present in temporary object but
         * present in current, and if there is default value that is the same.
         */

        const auto curAttrs = currentObj->getAllAttributes();

        for (auto curAttr: curAttrs)
        {
            if (attrs.find(curAttr.first) != attrs.end())
            {
                // attr exists in both objects.
                continue;
            }

            const sai_attr_metadata_t* meta = curAttr.second->getAttrMetadata();

            if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags) && !temporaryObj->hasAttr(curAttr.first))
            {
                /*
                 * This attribute exists only on current view and it's
                 * create only.  If it has default value, check if it's the
                 * same as current.
                 */

                auto tmpDefault = getSaiAttrFromDefaultValue(m_temporaryView, m_switch, *meta);

                if (tmpDefault != nullptr)
                {
                    if (tmpDefault->getStrAttrValue() != curAttr.second->getStrAttrValue())
                    {
                        has_different_create_only_attr = true;

                        SWSS_LOG_INFO("obj has not equal create only attributes %s (default): %s",
                                currentObj->m_str_object_id.c_str(),
                                meta->attridname);
                        break;
                    }
                    else
                    {
                        SWSS_LOG_INFO("obj has equal create only value %s (default): %s",
                                temporaryObj->m_str_object_id.c_str(),
                                meta->attridname);
                    }
                }
            }
        }

        if (has_different_create_only_attr)
        {
            /*
             * Those objects differs with attribute which is marked as
             * CREATE_ONLY so we will not be able to update current if
             * necessary using SET operations.
             */

            continue;
        }

        candidateObjects.push_back(soci);
    }

    SWSS_LOG_INFO("number candidate objects for %s is %zu",
            temporaryObj->m_str_object_id.c_str(),
            candidateObjects.size());

    if (candidateObjects.size() == 0)
    {
        /*
         * We didn't found any object.
         */

        return nullptr;
    }

    if (candidateObjects.size() == 1)
    {
        /*
         * We found only one object so it must be it.
         */

        return candidateObjects.begin()->obj;
    }

    auto labelCandidate = findCurrentBestMatchForGenericObjectUsingLabel(
            temporaryObj,
            candidateObjects);

    if (labelCandidate != nullptr)
        return labelCandidate;

    /*
     * If we have more than 1 object matched actually more preferred
     * object would be the object with most CREATE_ONLY attributes matching
     * since that will reduce risk of removing and recreating that object in
     * current view.
     */

    /*
     * But at this point also let's try find best candidate using graph paths,
     * since if some attributes are mismatched (like for example more ACLs are
     * created) this can lead to choose wrong LAG and have implications on
     * router interface and so on.  So matching by graph path here could be
     * more precise.
     */

    auto graphCandidate = findCurrentBestMatchForGenericObjectUsingGraph(
            temporaryObj,
            candidateObjects);

    if (graphCandidate != nullptr)
        return graphCandidate;

    /*
     * Sort candidate objects by equal attributes in descending order, we know
     * here that we have at least 2 candidates.
     *
     * NOTE: maybe at this point we should be using heuristics?
     */

    std::sort(candidateObjects.begin(), candidateObjects.end(), compareByEqualAttributes);

    if (candidateObjects.at(0).equal_attributes > candidateObjects.at(1).equal_attributes)
    {
        /*
         * We have only 1 object with the greatest number of equal attributes
         * lets choose that object as our best match.
         */

        SWSS_LOG_INFO("eq attributes: %ld vs %ld",
            candidateObjects.at(0).equal_attributes,
            candidateObjects.at(1).equal_attributes);

        return candidateObjects.begin()->obj;
    }

    /*
     * In here there are at least 2 current objects that have the same
     * number of equal attributes. In here we can do two things
     *
     * - select object at random, or
     * - use heuristic/smart lookup for inside graph
     *
     * Smart lookup would be for example searching whether current object is
     * pointing to the same PORT as temporary object (since ports are matched
     * at the beginning). For different types of objects we need different type
     * of logic and we can start adding that when needed and when missing we
     * will just choose at random possible causing some remove/recreate but
     * this logic is not perfect at this point.
     */

    /*
     * Lets also remove candidates with less equal attributes
     */

    size_t previousCandidates = candidateObjects.size();

    size_t equalAttributes = candidateObjects.at(0).equal_attributes;

    auto endIt = std::remove_if(candidateObjects.begin(), candidateObjects.end(),
            [equalAttributes](const sai_object_compare_info_t &candidate)
            { return candidate.equal_attributes != equalAttributes; });

    candidateObjects.erase(endIt, candidateObjects.end());

    SWSS_LOG_INFO("multiple candidates found (%zu of %zu) for %s, will use heuristic",
            candidateObjects.size(),
            previousCandidates,
            temporaryObj->m_str_object_id.c_str());

    return findCurrentBestMatchForGenericObjectUsingHeuristic(temporaryObj, candidateObjects);
}

bool BestCandidateFinder::compareByEqualAttributes(
        _In_ const sai_object_compare_info_t &a,
        _In_ const sai_object_compare_info_t &b)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: this function will sort in descending order
     */

    return a.equal_attributes > b.equal_attributes;
}

/**
 * @brief Select random SAI object from best candidates we found.
 *
 * Input list must contain at least one candidate.
 *
 * @param candidateObjects List of candidate objects.
 *
 * @return Random selected object from provided list.
 */
std::shared_ptr<SaiObj> BestCandidateFinder::selectRandomCandidate(
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    size_t candidateCount = candidateObjects.size();

    SWSS_LOG_INFO("selecting random candidate from %zu objects", candidateCount);

    size_t index = std::rand() % candidateCount;

    return candidateObjects.at(index).obj;
}

/**
 * @brief Find current best match for neighbor.
 *
 * For Neighbor we don't need to iterate via all current neighbors, we can do
 * dictionary lookup, but we need to do smart trick, since temporary object was
 * processed we just need to check whether VID in neighbor_entry struct is
 * matched/final and it has RID assigned from current view. If, RID exists, we
 * can use that RID to get VID of current view, exchange in neighbor_entry
 * struct and do dictionary lookup on serialized neighbor_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid neighbor entry, so we
 * must return null.
 *
 * @param m_currentView Current view.
 * @param m_temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForNeighborEntry(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->m_meta_key;

    if (!exchangeTemporaryVidToCurrentVid(mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(mk.objectkey.key.neighbor_entry);

    /*
     * Now when we have serialized neighbor entry with temporary rif_if VID
     * replaced to current rif_id VID we can do dictionary lookup for neighbor.
     */

    auto currentNeighborIt = m_currentView.m_soNeighbors.find(str_neighbor_entry);

    if (currentNeighborIt == m_currentView.m_soNeighbors.end())
    {
        SWSS_LOG_DEBUG("unable to find neighbor entry %s in current asic view",
                str_neighbor_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same neighbor entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentNeighborObj = currentNeighborIt->second;

    if (currentNeighborObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentNeighborObj;
    }

    /*
     * If we are here, that means this neighbor was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found neighbor entry %s in current view, but it status is %d, FATAL",
            str_neighbor_entry.c_str(),
            currentNeighborObj->getObjectStatus());
}

/**
 * @brief Find current best match for route.
 *
 * For Route we don't need to iterate via all current routes, we can do
 * dictionary lookup, but we need to do smart trick, since temporary object was
 * processed we just need to check whether VID in route_entry struct is
 * matched/final and it has RID assigned from current view. If, RID exists, we
 * can use that RID to get VID of current view, exchange in route_entry struct
 * and do dictionary lookup on serialized route_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid route entry, so we
 * must return null.
 *
 * @param m_currentView Current view.
 * @param m_temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForRouteEntry(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->m_meta_key;

    if (!exchangeTemporaryVidToCurrentVid(mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_route_entry = sai_serialize_route_entry(mk.objectkey.key.route_entry);

    /*
     * Now when we have serialized route entry with temporary vr_id VID
     * replaced to current vr_id VID we can do dictionary lookup for route.
     */
    auto currentRouteIt = m_currentView.m_soRoutes.find(str_route_entry);

    if (currentRouteIt == m_currentView.m_soRoutes.end())
    {
        SWSS_LOG_DEBUG("unable to find route entry %s in current asic view", str_route_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same route entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentRouteObj = currentRouteIt->second;

    if (currentRouteObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentRouteObj;
    }

    /*
     * If we are here, that means this route was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found route entry %s in current view, but it status is %d, FATAL",
            str_route_entry.c_str(),
            currentRouteObj->getObjectStatus());
}

/**
 * @brief Find current best match for inseg.
 *
 * For Inseg we don't need to iterate via all current insegs, we can do
 * dictionary lookup, but we need to do smart trick, since temporary object was
 * processed we just need to check whether VID in inseg_entry struct is
 * matched/final and it has RID assigned from current view. If, RID exists, we
 * can use that RID to get VID of current view, exchange in inseg_entry struct
 * and do dictionary lookup on serialized inseg_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid inseg entry, so we
 * must return null.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForInsegEntry(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->m_meta_key;

    if (!exchangeTemporaryVidToCurrentVid(mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_inseg_entry = sai_serialize_inseg_entry(mk.objectkey.key.inseg_entry);

    /*
     * Now when we have serialized inseg entry with temporary vr_id VID
     * replaced to current vr_id VID we can do dictionary lookup for inseg.
     */
    auto currentInsegIt = m_currentView.m_soInsegs.find(str_inseg_entry);

    if (currentInsegIt == m_currentView.m_soInsegs.end())
    {
        SWSS_LOG_DEBUG("unable to find inseg entry %s in current asic view", str_inseg_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same inseg entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentInsegObj = currentInsegIt->second;

    if (currentInsegObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentInsegObj;
    }

    /*
     * If we are here, that means this inseg was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found inseg entry %s in current view, but it status is %d, FATAL",
            str_inseg_entry.c_str(),
            currentInsegObj->getObjectStatus());
}

/**
 * @brief Find current best match for FDB.
 *
 * For FDB we don't need to iterate via all current FDBs, we can do dictionary
 * lookup, but we need to do smart trick, since temporary object was processed
 * we just need to check whether VID in fdb_entry struct is matched/final and
 * it has RID assigned from current view. If, RID exists, we can use that RID
 * to get VID of current view, exchange in fdb_entry struct and do dictionary
 * lookup on serialized fdb_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid fdb entry, so we must
 * return null.
 *
 * @param m_currentView Current view.
 * @param m_temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForFdbEntry(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->m_meta_key;

    if (!exchangeTemporaryVidToCurrentVid(mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_fdb_entry = sai_serialize_fdb_entry(mk.objectkey.key.fdb_entry);

    /*
     * Now when we have serialized fdb entry with temporary VIDs
     * replaced to current VIDs we can do dictionary lookup for fdb.
     */

    auto currentFdbIt = m_currentView.m_soFdbs.find(str_fdb_entry);

    if (currentFdbIt == m_currentView.m_soFdbs.end())
    {
        SWSS_LOG_DEBUG("unable to find fdb entry %s in current asic view", str_fdb_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same fdb entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentFdbObj = currentFdbIt->second;

    if (currentFdbObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentFdbObj;
    }

    /*
     * If we are here, that means this fdb was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found fdb entry %s in current view, but it status is %d, FATAL",
            str_fdb_entry.c_str(),
            currentFdbObj->getObjectStatus());
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForSwitch(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * On compare logic we checked that we have one switch
     * so we can just get first.
     */

    auto currentSwitches = m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_SWITCH);

    if (currentSwitches.size() == 0)
    {
        SWSS_LOG_NOTICE("unable to find switch object in current view");

        return nullptr;
    }

    if (currentSwitches.size() > 1)
    {
        SWSS_LOG_THROW("found %zu switches declared in current view, not supported",
                currentSwitches.size());
    }

    /*
     * We found switch object in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentSwitchObj = currentSwitches.at(0);

    if (currentSwitchObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentSwitchObj;
    }

    /*
     * If we are here, that means this switch was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found switch object %s in current view, but it status is %d, FATAL",
            currentSwitchObj->m_str_object_id.c_str(),
            currentSwitchObj->getObjectStatus());
}

/**
 * @brief Find current best match for NAT entry.
 *
 *
 * @param m_currentView Current view.
 * @param m_temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForNatEntry(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->m_meta_key;

    if (!exchangeTemporaryVidToCurrentVid(mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_nat_entry = sai_serialize_nat_entry(mk.objectkey.key.nat_entry);

    /*
     * Now when we have serialized NAT entry with temporary vr_id VID
     * replaced to current vr_id VID we can do dictionary lookup for NAT entry.
     */
    auto currentNatEntry = m_currentView.m_soNatEntries.find(str_nat_entry);

    if (currentNatEntry == m_currentView.m_soNatEntries.end())
    {
        SWSS_LOG_DEBUG("unable to find NAT entry %s in current asic view", str_nat_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same NAT entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentNatObj = currentNatEntry->second;

    if (currentNatObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentNatObj;
    }


    /*
     * If we are here, that means this NAT entry was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found NAT entry %s in current view, but it status is %d, FATAL",
            str_nat_entry.c_str(),
            currentNatObj->getObjectStatus());
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatch(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    if (temporaryObj->isOidObject() &&
            temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
    {
        /*
         * Object status is matched so current and temp VID are the same so we
         * can just take object directly.
         */

        SWSS_LOG_INFO("found best match for %s %s since object status is MATCHED",
                temporaryObj->m_str_object_type.c_str(),
                temporaryObj->m_str_object_id.c_str());

        return m_currentView.m_oOids.at(temporaryObj->getVid());
    }

    /*
     * NOTE: Later on best match find can be done based on present CREATE_ONLY
     * attributes or on most matched CREATE_AND_SET attributes.
     */

    switch (temporaryObj->getObjectType())
    {
        /*
         * Non object id cases
         */

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return findCurrentBestMatchForNeighborEntry(temporaryObj);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return findCurrentBestMatchForRouteEntry(temporaryObj);

        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return findCurrentBestMatchForFdbEntry(temporaryObj);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return findCurrentBestMatchForNatEntry(temporaryObj);

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
            return findCurrentBestMatchForInsegEntry(temporaryObj);

            /*
             * We can have special case for switch since we know there should
             * be only one switch.
             */

        case SAI_OBJECT_TYPE_SWITCH:
            return findCurrentBestMatchForSwitch(temporaryObj);

        default:

            if (!temporaryObj->isOidObject())
            {
                SWSS_LOG_THROW("object %s:%s is non object id, not handled yet, FIXME",
                        temporaryObj->m_str_object_type.c_str(),
                        temporaryObj->m_str_object_id.c_str());
            }

            /*
             * Here we support only object id object types.
             */

            return findCurrentBestMatchForGenericObject(temporaryObj);
    }
}

std::shared_ptr<SaiObj> BestCandidateFinder::findSimilarBestMatch(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    // will find object in current view that have most same attributes

    auto notProcessedObjects = m_currentView.getNotProcessedObjectsByObjectType(temporaryObj->getObjectType());

    const auto attrs = temporaryObj->getAllAttributes();

    SWSS_LOG_INFO("not processed objects for %s: %zu, temp attrs: %zu",
            temporaryObj->m_str_object_type.c_str(),
            notProcessedObjects.size(),
            attrs.size());

    std::vector<sai_object_compare_info_t> candidateObjects;

    for (const auto &currentObj: notProcessedObjects)
    {
        // log how many attributes current object have

        SWSS_LOG_INFO("current obj %s, attrs: %zu",
                currentObj->m_str_object_id.c_str(),
                currentObj->getAllAttributes().size());

        sai_object_compare_info_t soci = { 0, currentObj };

        /*
         * NOTE: we only iterate by attributes that are present in temporary
         * view. It may happen that current view has some additional attributes
         * set that are create only and value can't be updated, we ignore that
         * since we want to find object with most matching attributes.
         */

        for (const auto &attr: attrs)
        {
            sai_attr_id_t attrId = attr.first;

            // Function hasEqualAttribute check if attribute exists on both objects.

            if (hasEqualAttribute(m_currentView, m_temporaryView, currentObj, temporaryObj, attrId))
            {
                soci.equal_attributes++;

                SWSS_LOG_INFO("ob equal %s %s, %s: %s",
                        temporaryObj->m_str_object_id.c_str(),
                        currentObj->m_str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());
            }
            else
            {
                SWSS_LOG_INFO("ob not equal %s %s, %s: %s",
                        temporaryObj->m_str_object_id.c_str(),
                        currentObj->m_str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());
            }
        }

        // NOTE: we could check if current object has some attributes that are
        // default value on temporary object, and count them in

        candidateObjects.push_back(soci);
    }

    SWSS_LOG_INFO("number candidate objects for %s is %zu",
            temporaryObj->m_str_object_id.c_str(),
            candidateObjects.size());

    if (candidateObjects.size() == 0)
    {
        SWSS_LOG_WARN("not processed objects in current view is zero");

        return nullptr;
    }

    if (candidateObjects.size() == 1)
    {
        // We found only one object so return it as candidate

        return candidateObjects.begin()->obj;
    }

    /*
     * Sort candidate objects by equal attributes in descending order, we know
     * here that we have at least 2 candidates.
     */

    std::sort(candidateObjects.begin(), candidateObjects.end(), compareByEqualAttributes);

    if (candidateObjects.at(0).equal_attributes > candidateObjects.at(1).equal_attributes)
    {
        /*
         * We have only 1 object with the greatest number of equal attributes
         * lets choose that object as our best match.
         */

        SWSS_LOG_INFO("eq attributes: %ld vs %ld",
            candidateObjects.at(0).equal_attributes,
            candidateObjects.at(1).equal_attributes);

        return candidateObjects.begin()->obj;
    }

    SWSS_LOG_WARN("same number of attributes equal, selecting first");

    return candidateObjects.begin()->obj;
}

int BestCandidateFinder::findAllChildsInDependencyTreeCount(
        _In_ const AsicView &view,
        _In_ const std::shared_ptr<const SaiObj> &obj)
{
    SWSS_LOG_ENTER();

    int count = 0;

    const auto &info = obj->m_info;

    if (info->revgraphmembers == NULL)
    {
        return count;
    }

    for (int idx = 0; info->revgraphmembers[idx] != NULL; ++idx)
    {
        auto &member = info->revgraphmembers[idx];

        if (member->attrmetadata == NULL)
        {
            /*
             * Skip struct members for now but we need support.
             */

            continue;
        }

        auto &meta = member->attrmetadata;

        if (SAI_HAS_FLAG_READ_ONLY(meta->flags))
        {
            /*
             * Skip read only attributes.
             */

            continue;
        }

        if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            /*
             * For now skip lists and acl.
             */

            continue;
        }

        SWSS_LOG_DEBUG("looking for %s on %s", obj->m_str_object_type.c_str(), meta->attridname);

        auto usageObjects = findUsageCount(view, obj, meta->objecttype, meta->attrid);

        count += (int)usageObjects.size();

        if (meta->objecttype == obj->getObjectType())
        {
            /*
             * Skip loops on scheduler group.
             * Mirror session has loop on port, but we break port in next condition.
             */

            continue;
        }

        if (meta->objecttype == SAI_OBJECT_TYPE_PORT ||
                meta->objecttype == SAI_OBJECT_TYPE_SWITCH)
        {
            /*
             * Ports and switch have a lot of members lets just stop here.
             */
            continue;
        }

        /*
         * For each object that is using input object run recursion to find all
         * nodes in the tree.
         *
         * NOTE: if we would have actual tree then this task would be much
         * easier.
         */

        for (auto &uo: usageObjects)
        {
            count += findAllChildsInDependencyTreeCount(view, uo);
        }
    }

    return count;
}

/**
 * @brief Check if current and temporary object has
 * the same attribute and attribute has the same value on both.
 *
 * This also includes object ID attributes, that's why we need
 * current and temporary view to compare RID values.
 *
 * NOTE: both objects must be the same object type, otherwise
 * this compare make no sense.
 *
 * NOTE: this function does not check if attributes are
 * different, whether we can update existing one to new one,
 * for that we will need different method
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param current Current object.
 * @param temporary Temporary object.
 * @param id Attribute id to be checked.
 *
 * @return True if attributes are equal, false otherwise.
 */
bool BestCandidateFinder::hasEqualAttribute(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &current,
        _In_ const std::shared_ptr<const SaiObj> &temporary,
        _In_ sai_attr_id_t id)
{
    SWSS_LOG_ENTER();

    /*
     * Currently we only check if both attributes exists on both objects.
     *
     * One of them maybe missing, if it has default value and the values still
     * maybe the same so in that case we should/could also return true.
     */

    if (current->hasAttr(id) && temporary->hasAttr(id))
    {
        const auto &currentAttr = current->getSaiAttr(id);
        const auto &temporaryAttr = temporary->getSaiAttr(id);

        if (currentAttr->getAttrMetadata()->attrvaluetype == SAI_ATTR_VALUE_TYPE_POINTER)
        {
            auto c = currentAttr->getSaiAttr()->value.ptr;
            auto t = temporaryAttr->getSaiAttr()->value.ptr;

            /*
             * When comparing pointers, actual value's can't be checked since
             * actual value is pointer in sairedis/OA address space. Syncd
             * translates those pointers before executing SAI API. Pointers are
             * considered equal if they are both null, or both not null since
             * the same handler method in syncd is used.
             */

            if (c == nullptr && t == nullptr)
                return true;

            if (c != nullptr && t != nullptr)
                return true;

            SWSS_LOG_NOTICE("current ptr on %s is %p, tmp ptr is %p", currentAttr->getAttrMetadata()->attridname, c, t);

            return false;
        }

        if (currentAttr->getStrAttrValue() == temporaryAttr->getStrAttrValue())
        {
            /*
             * Serialized value of the attributes are equal so attributes
             * must be equal, this is even true for object ID attributes
             * since this will be only true if VID in both attributes are
             * the same, and if VID's are the same then RID's are also
             * the same, so no need to actual RID's compare.
             */

            /*
             * Worth noticing here that acl entry field/action have enable
             * flag, and if it's disabled then parameter value don't matter.
             * But this is fine here since serialized value don't contain
             * parameter value if it's disabled so we don't need to take extra
             * care about this case here.
             */

            return true;
        }

        if (currentAttr->getAttrMetadata()->attrvaluetype == SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST)
        {
            /*
             * In case of qos map list, order of list does not matter, so
             * compare only entries.
             */

            return hasEqualQosMapList(currentAttr, temporaryAttr);
        }

        if (currentAttr->isObjectIdAttr() == false)
        {
            /*
             * This means attribute is primitive and don't contain
             * any object ids, so we can return false right away
             * instead of getting into switch below
             */

            return false;
        }

        /*
         * In this place we know that attribute values are different,
         * but if attribute serialize type is object id, their RID's
         * maybe equal, and that means actual attributes values
         * are equal as well, so we should return true in that case.
         */

        /*
         * We can use getOidListFromAttribute for extracting those oid lists
         * since we know that attribute type is oid type.
         */

        const auto &temporaryObjList = temporaryAttr->getOidListFromAttribute();
        const auto &currentObjList = currentAttr->getOidListFromAttribute();

        /*
         * This function already supports enable flags for acl field and action
         * so we don't need to worry about it here.
         *
         * TODO: For acl list/action this maybe wrong since one can be
         * enabled and list is empty and other one can be disabled and this
         * function also return empty, so this will mean they are equal but
         * they don't since they differ with enable flag. We probably won't hit
         * this, since we probably always have some oids on the list.
         */

        return hasEqualObjectList(
                currentView,
                temporaryView,
                (uint32_t)currentObjList.size(),
                currentObjList.data(),
                (uint32_t)temporaryObjList.size(),
                temporaryObjList.data());
    }

    /*
     * Currently we don't support case where only one attribute is present, but
     * we should consider that if other attribute has default value.
     */

    return false;
}

std::shared_ptr<SaiAttr> BestCandidateFinder::getSaiAttrFromDefaultValue(
        _In_ const AsicView &currentView,
        _In_ std::shared_ptr<const SaiSwitchInterface> sw,
        _In_ const sai_attr_metadata_t &meta)
{
    SWSS_LOG_ENTER();

    /*
     * Worth notice, that this is only helper, since metadata on attributes
     * tell default value for example for oid object as SAI_NULL_OBJECT_ID but
     * maybe on the switch vendor actually assigned some value, so default
     * value will not be NULL after creation.
     *
     * We can check that by using SAI discovery.
     *
     * TODO Default value also must depend on dependency tree !
     * This will be tricky, we need to revisit that !
     */

    if (meta.objecttype == SAI_OBJECT_TYPE_SWITCH &&
            meta.attrid == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
    {
        /*
         * Same will apply for default values which are pointing to
         * different attributes.
         *
         * Default value is stored in SaiSwitch class.
         */

        // XXX we have only 1 switch, so we can get away with this

        sai_attribute_t attr;

        memset(&attr, 0, sizeof(sai_attribute_t));

        attr.id = meta.attrid;

        sw->getDefaultMacAddress(attr.value.mac);

        std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

        SWSS_LOG_NOTICE("bringing default %s", meta.attridname);

        return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
    }

    /*
     * Move this method to asicview class.
     */

    switch (meta.defaultvaluetype)
    {
        case SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST:

            {
                /*
                 * For empty list we can use trick here, just set attr to all
                 * zeros, then all count's will be zeros and all lists pointers
                 * will be NULL.
                 *
                 * TODO: If this is executed on existing attribute then we need
                 * Dependency tree since initially that list may not be empty!
                 */

                sai_attribute_t attr;

                memset(&attr, 0, sizeof(sai_attribute_t));

                attr.id = meta.attrid;

                std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

                return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
            }


        case SAI_DEFAULT_VALUE_TYPE_CONST:

            /*
             * Only primitives can be supported on CONST.
             */

            switch (meta.attrvaluetype)
            {
                case SAI_ATTR_VALUE_TYPE_BOOL:
                case SAI_ATTR_VALUE_TYPE_UINT8:
                case SAI_ATTR_VALUE_TYPE_INT8:
                case SAI_ATTR_VALUE_TYPE_UINT16:
                case SAI_ATTR_VALUE_TYPE_INT16:
                case SAI_ATTR_VALUE_TYPE_UINT32:
                case SAI_ATTR_VALUE_TYPE_INT32:
                case SAI_ATTR_VALUE_TYPE_UINT64:
                case SAI_ATTR_VALUE_TYPE_INT64:
                case SAI_ATTR_VALUE_TYPE_POINTER:
                case SAI_ATTR_VALUE_TYPE_OBJECT_ID:

                    {
                        sai_attribute_t attr;

                        attr.id = meta.attrid;
                        attr.value = *meta.defaultvalue;

                        std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

                        return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
                    }

                default:

                    SWSS_LOG_ERROR("serialization type %s is not supported yet, FIXME",
                            sai_serialize_attr_value_type(meta.attrvaluetype).c_str());
                    break;
            }

            /*
             * NOTE: default for acl flags or action is disabled.
             */

            break;

        case SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE:

            /*
             * TODO normally we need check default object type and value but
             * this is only available in metadata sai 1.0.
             *
             * We need to look at attrvalue in metadata and object type if it's switch.
             * then get it from switch
             *
             * And all those values we should keep in double map object type
             * and attribute id, and auto select from attr value.
             */

            // TODO double check that, to make it generic (check attr value type is oid)
            if (meta.objecttype == SAI_OBJECT_TYPE_HOSTIF_TRAP && meta.attrid == SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP)
            {
                /*
                 * Default trap group is set on traps by default, so to bring them to
                 * default state we need to explicitly set this.
                 */

                SWSS_LOG_NOTICE("bring default trap group on %s", meta.attridname);

                const auto &tg = currentView.m_ridToVid.find(currentView.m_defaultTrapGroupRid);

                if (tg == currentView.m_ridToVid.end())
                {
                    SWSS_LOG_THROW("default trap group RID 0x%" PRIx64 " doesn't exist in current view", currentView.m_defaultTrapGroupRid);
                }

                sai_attribute_t at;

                at.id = meta.attrid;
                at.value.oid = tg->second; // default trap group VID

                std::string str_attr_value = sai_serialize_attr_value(meta, at, false);

                return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
            }

            SWSS_LOG_ERROR("default value type %d is not supported yet for %s, FIXME",
                    meta.defaultvaluetype,
                    meta.attridname);

            return nullptr;

        case SAI_DEFAULT_VALUE_TYPE_NONE:

            /*
             * No default value present.
             */

            return nullptr;

        default:

            SWSS_LOG_ERROR("default value type %d is not supported yet for %s, FIXME",
                    meta.defaultvaluetype,
                    meta.attridname);

            return nullptr;
    }

    return nullptr;
}

bool BestCandidateFinder::exchangeTemporaryVidToCurrentVid(
        _Inout_ sai_object_meta_key_t &meta_key)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    // XXX if (info->isobjectid)

    if (!info->isnonobjectid)
    {
        SWSS_LOG_THROW("expected non object id, but got: %s",
                sai_serialize_object_meta_key(meta_key).c_str());
    }

    for (size_t j = 0; j < info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = info->structmembers[j];

        if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            continue;
        }

        sai_object_id_t tempVid = m->getoid(&meta_key);

        auto temporaryIt = m_temporaryView.m_vidToRid.find(tempVid);

        if (temporaryIt == m_temporaryView.m_vidToRid.end())
        {
            /*
             * RID for temporary VID was not assigned, so we didn't matched it
             * in previous processing, or VID will be created later on. This
             * mean we can't match because even if all attributes are matching,
             * RID in struct after create will not match so at the end of
             * processing current object will need to be destroyed and new one
             * needs to be created.
             */

            return false;
        }

        sai_object_id_t temporaryRid = temporaryIt->second;

        auto currentIt = m_currentView.m_ridToVid.find(temporaryRid);

        if (currentIt == m_currentView.m_ridToVid.end())
        {
            /*
             * This is just sanity check, should never happen.
             */

            SWSS_LOG_THROW("found temporary RID %s but current VID doesn't exist, FATAL",
                    sai_serialize_object_id(temporaryRid).c_str());
        }

        /*
         * This vid is vid of current view, it may or may not be
         * equal to temporaryVid, but we need to this step to not guess vids.
         */

        sai_object_id_t currentVid = currentIt->second;

        m->setoid(&meta_key, currentVid);
    }

    return true;
}

std::vector<std::shared_ptr<const SaiObj>> BestCandidateFinder::findUsageCount(
        _In_ const AsicView &view,
        _In_ const std::shared_ptr<const SaiObj> &obj,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<const SaiObj>> filtered;

    const auto &members = view.getObjectsByObjectType(object_type);

    for (auto &m: members)
    {
        if (m->hasAttr(attr_id))
        {
            const auto &attr = m->getSaiAttr(attr_id);

            if (attr->getAttrMetadata()->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                SWSS_LOG_THROW("attribute %s is not oid attribute, or not oid attr, bug!",
                        attr->getAttrMetadata()->attridname);
            }

            if (attr->getSaiAttr()->value.oid == obj->getVid())
            {
                filtered.push_back(m);
            }
        }
    }

    return filtered;
}

/**
 * @brief Check if both list contains the same objects
 *
 * Function returns TRUE only when all list contain exact the same objects
 * compared by RID values and with exact same order.
 *
 * TODO Currently order on the list matters, but we need to update this logic
 * so order will not matter, just values of object will need to be considered.
 * We need to have extra index of processed objects and not processed yet.  We
 * should also cover NULL case and duplicated objects.  Normally we should not
 * have duplicated object id's on the list, and we can easy check that using
 * hash.
 *
 * In case of really long list, easier way to solve this can be getting all the
 * RIDs from current view (they must exist), getting all the matched RIDs from
 * temporary list (if one of them doesn't exist then lists are not equal) sort
 * both list nlog(n) and then compare sequentially.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param current_count Object list count from current view.
 * @param current_list Object list from current view.
 * @param temporary_count Object list count from temporary view.
 * @param temporary_list Object list from temporary view.
 *
 * @return True if object list are equal, false otherwise.
 */
bool BestCandidateFinder::hasEqualObjectList(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ uint32_t current_count,
        _In_ const sai_object_id_t *current_list,
        _In_ uint32_t temporary_count,
        _In_ const sai_object_id_t *temporary_list)
{
    SWSS_LOG_ENTER();

    if (current_count != temporary_count)
    {
        /*
         * Length of lists are not equal, so lists are different.
         */

        return false;
    }

    for (uint32_t idx = 0; idx < current_count; ++idx)
    {
        sai_object_id_t currentVid = current_list[idx];
        sai_object_id_t temporaryVid = temporary_list[idx];

        if (currentVid == SAI_NULL_OBJECT_ID &&
                temporaryVid == SAI_NULL_OBJECT_ID)
        {
            /*
             * Both current and temporary are the same so we
             * continue for next item on list.
             */

            continue;
        }

        if (currentVid != SAI_NULL_OBJECT_ID &&
                temporaryVid != SAI_NULL_OBJECT_ID)
        {
            /*
             * Check for object type of both objects, they must
             * match.  But maybe this is not necessary, since true
             * is returned only when RIDs match, so it's more like
             * sanity check.
             */

            sai_object_type_t temporaryObjectType = VidManager::objectTypeQuery(temporaryVid);
            sai_object_type_t currentObjectType = VidManager::objectTypeQuery(currentVid);

            if (temporaryObjectType == SAI_OBJECT_TYPE_NULL ||
                    currentObjectType == SAI_OBJECT_TYPE_NULL)
            {
                /*
                 * This case should never happen, we always should
                 * be able to extract valid object type from any
                 * VID, if this happens then we have a bug.
                 */

                SWSS_LOG_THROW("temporary object type is %s and current object type is %s, FATAL",
                        sai_serialize_object_type(temporaryObjectType).c_str(),
                        sai_serialize_object_type(currentObjectType).c_str());
            }

            if (temporaryObjectType != currentObjectType)
            {
                /*
                 * Compared object types are different, so they can't be equal.
                 * No need to check other objects on list.
                 */

                return false;
            }

            auto temporaryIt = temporaryView.m_vidToRid.find(temporaryVid);

            if (temporaryIt == temporaryView.m_vidToRid.end())
            {
                /*
                 * Temporary RID doesn't exist yet for this object, so it means
                 * this object will be created in the future after all
                 * comparison logic finishes.
                 *
                 * Here we know that this temporary object is not processed yet
                 * but during recursive processing we know that this OID value
                 * was already processed, and two things could happen:
                 *
                 * - we matched existing current object for this VID and actual
                 *   RID was assigned, or
                 *
                 * - during matching we didn't matched current object so RID is
                 *   not assigned, and this object will be created later on
                 *   which will assign new RID
                 *
                 * Since we are here where RID doesn't exist this is the second
                 * case, also we know that current object VID exists so his RID
                 * also exists, so those RID's can't be equal, we need return
                 * false here.
                 *
                 * Fore more strong verification we can introduce and extra
                 * flag in SaiObj indicating that object was processed and it
                 * needs to be created.
                 */

                SWSS_LOG_INFO("temporary RID doesn't exist (VID %s), attributes are not equal",
                        sai_serialize_object_id(temporaryVid).c_str());

                return false;
            }

            /*
             * Current VID exists, so current RID also must exists but let's
             * put sanity check here just in case if we mess something up, this
             * should never happen.
             */

            auto currentIt = currentView.m_vidToRid.find(currentVid);

            if (currentIt == currentView.m_vidToRid.end())
            {
                SWSS_LOG_THROW("current VID %s exists but current RID is missing, FATAL",
                        sai_serialize_object_id(currentVid).c_str());
            }

            sai_object_id_t temporaryRid = temporaryIt->second;
            sai_object_id_t currentRid = currentIt->second;

            /*
             * If RID's are equal, then object attribute values are equal as well.
             */

            if (temporaryRid == currentRid)
            {
                continue;
            }

            /*
             * If RIDs are different, then list are not equal.
             */

            return false;
        }

        /*
         * If we are here that means one of attributes value OIDs
         * is NULL and other is not, so they are not equal we need
         * to return false.
         */

        return false;
    }

    /*
     * We processed all objects on both lists, and they all are equal so both
     * list are equal even if they are empty. We need to return true in this
     * case.
     */

    return true;
}

/**
 * @brief Compare qos map list attributes order insensitive.
 *
 * @param current Current object qos map attribute.
 * @param temporary Temporary object qos map attribute.
 *
 * @return True if attributes are equal, false otherwise.
 */
bool BestCandidateFinder::hasEqualQosMapList(
        _In_ const sai_qos_map_list_t& c,
        _In_ const sai_qos_map_list_t& t)
{
    SWSS_LOG_ENTER();

    if (c.count != t.count)
        return false;

    if (c.list == NULL || t.list == NULL)
        return false;

    std::vector<std::string> citems;
    std::vector<std::string> titems;

    for (uint32_t i = 0; i < c.count; i++)
    {
        citems.push_back(sai_serialize_qos_map_item(c.list[i]));
        titems.push_back(sai_serialize_qos_map_item(t.list[i]));
    }

    std::sort(citems.begin(), citems.end());
    std::sort(titems.begin(), titems.end());

    for (uint32_t i = 0; i < c.count; i++)
    {
        if (citems.at(i) != titems.at(i))
        {
            return false;
        }
    }

    SWSS_LOG_NOTICE("qos map are equal, but has different order");

    // all items in both attributes are equal
    return true;
}

bool BestCandidateFinder::hasEqualQosMapList(
        _In_ const std::shared_ptr<const SaiAttr> &current,
        _In_ const std::shared_ptr<const SaiAttr> &temporary)
{
    SWSS_LOG_ENTER();

    auto c = current->getSaiAttr()->value.qosmap;
    auto t = temporary->getSaiAttr()->value.qosmap;

    return hasEqualQosMapList(c, t);
}


