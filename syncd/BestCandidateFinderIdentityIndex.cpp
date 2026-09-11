#include "BestCandidateFinder.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

#include <algorithm>
#include <sstream>

using namespace syncd;

std::set<sai_attr_id_t> BestCandidateFinder::buildAttrFilterFromObject(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    std::set<sai_attr_id_t> filter;

    for (const auto &kvp: temporaryObj->getAllAttributes())
    {
        if (AsicView::isAttrIncludedInIdentityKey(kvp.second->getAttrMetadata()))
        {
            filter.insert(kvp.first);
        }
    }

    return filter;
}

bool BestCandidateFinder::isSlowPathMatchType(
        _In_ sai_object_type_t objectType)
{
    SWSS_LOG_ENTER();

    return objectType == SAI_OBJECT_TYPE_NEXT_HOP ||
           objectType == SAI_OBJECT_TYPE_NEXT_HOP_GROUP ||
           objectType == SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER;
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchUsingIdentityIndex(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    const sai_object_type_t objectType = temporaryObj->getObjectType();

    const std::set<sai_attr_id_t> attrFilter = buildAttrFilterFromObject(temporaryObj);

    if (attrFilter.empty())
    {
        return nullptr;
    }

    const auto *index = m_currentView.getCreateOnlyIndex(objectType, attrFilter);

    if (index == nullptr)
    {
        return nullptr;
    }

    const std::string key = AsicView::computeCreateOnlyKey(
            temporaryObj,
            m_temporaryView.m_vidToRid,
            &attrFilter);

    if (key.empty())
    {
        return nullptr;
    }

    const auto it = index->find(key);

    if (it == index->end())
    {
        return nullptr;
    }

    if (it->second->getObjectStatus() != SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return nullptr;
    }

    SWSS_LOG_NOTICE("identity index match for %s -> %s",
            temporaryObj->m_str_object_id.c_str(),
            it->second->m_str_object_id.c_str());

    return it->second;
}

sai_object_id_t BestCandidateFinder::resolveNextHopRidForNhgKey(
        _In_ sai_object_id_t nhVid)
{
    SWSS_LOG_ENTER();

    if (nhVid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto tmpIt = m_temporaryView.m_vidToRid.find(nhVid);

    if (tmpIt != m_temporaryView.m_vidToRid.end())
    {
        return tmpIt->second;
    }

    auto nhObjIt = m_temporaryView.m_oOids.find(nhVid);

    if (nhObjIt == m_temporaryView.m_oOids.end())
    {
        return SAI_NULL_OBJECT_ID;
    }

    const auto &tempNh = nhObjIt->second;

    const std::set<sai_attr_id_t> attrFilter = buildAttrFilterFromObject(tempNh);

    if (!attrFilter.empty())
    {
        const std::string nhKey = AsicView::computeCreateOnlyKey(
                tempNh,
                m_temporaryView.m_vidToRid,
                &attrFilter);

        if (!nhKey.empty())
        {
            const auto *nhIndex = m_currentView.getCreateOnlyIndex(SAI_OBJECT_TYPE_NEXT_HOP, attrFilter);

            if (nhIndex != nullptr)
            {
                const auto nhIt = nhIndex->find(nhKey);

                if (nhIt != nhIndex->end())
                {
                    auto curVid = nhIt->second->getVid();
                    auto curIt = m_currentView.m_vidToRid.find(curVid);

                    if (curIt != m_currentView.m_vidToRid.end())
                    {
                        return curIt->second;
                    }
                }
            }
        }
    }

    if (!tempNh->hasAttr(SAI_NEXT_HOP_ATTR_TUNNEL_ID) || !tempNh->hasAttr(SAI_NEXT_HOP_ATTR_TYPE))
    {
        return SAI_NULL_OBJECT_ID;
    }

    sai_object_id_t tunnelVid = tempNh->getSaiAttr(SAI_NEXT_HOP_ATTR_TUNNEL_ID)->getOid();
    sai_object_id_t tunnelRid = SAI_NULL_OBJECT_ID;

    auto tunnelTmpIt = m_temporaryView.m_vidToRid.find(tunnelVid);

    if (tunnelTmpIt != m_temporaryView.m_vidToRid.end())
    {
        tunnelRid = tunnelTmpIt->second;
    }
    else
    {
        auto tunnelObjIt = m_temporaryView.m_oOids.find(tunnelVid);

        if (tunnelObjIt != m_temporaryView.m_oOids.end())
        {
            const std::set<sai_attr_id_t> tunnelFilter = buildAttrFilterFromObject(tunnelObjIt->second);
            const std::string tunnelKey = AsicView::computeCreateOnlyKey(
                    tunnelObjIt->second,
                    m_temporaryView.m_vidToRid,
                    &tunnelFilter);

            if (!tunnelKey.empty())
            {
                const auto *tunnelIndex = m_currentView.getCreateOnlyIndex(SAI_OBJECT_TYPE_TUNNEL, tunnelFilter);

                if (tunnelIndex != nullptr)
                {
                    const auto tunnelIt = tunnelIndex->find(tunnelKey);

                    if (tunnelIt != tunnelIndex->end())
                    {
                        auto curTunnelVid = tunnelIt->second->getVid();
                        auto curTunnelIt = m_currentView.m_vidToRid.find(curTunnelVid);

                        if (curTunnelIt != m_currentView.m_vidToRid.end())
                        {
                            tunnelRid = curTunnelIt->second;
                        }
                    }
                }
            }
        }
    }

    if (tunnelRid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    const std::string nhType = tempNh->getSaiAttr(SAI_NEXT_HOP_ATTR_TYPE)->getStrAttrValue();

    std::vector<sai_object_id_t> matchingRids;

    for (const auto &curNh: m_currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_NEXT_HOP))
    {
        if (!curNh->hasAttr(SAI_NEXT_HOP_ATTR_TUNNEL_ID) || !curNh->hasAttr(SAI_NEXT_HOP_ATTR_TYPE))
        {
            continue;
        }

        if (curNh->getSaiAttr(SAI_NEXT_HOP_ATTR_TYPE)->getStrAttrValue() != nhType)
        {
            continue;
        }

        sai_object_id_t curTunnelVid = curNh->getSaiAttr(SAI_NEXT_HOP_ATTR_TUNNEL_ID)->getOid();
        auto curTunnelIt = m_currentView.m_vidToRid.find(curTunnelVid);

        if (curTunnelIt == m_currentView.m_vidToRid.end())
        {
            continue;
        }

        if (curTunnelIt->second != tunnelRid)
        {
            continue;
        }

        auto curNhIt = m_currentView.m_vidToRid.find(curNh->getVid());

        if (curNhIt != m_currentView.m_vidToRid.end())
        {
            matchingRids.push_back(curNhIt->second);
        }
    }

    if (matchingRids.empty())
    {
        return SAI_NULL_OBJECT_ID;
    }

    std::sort(matchingRids.begin(), matchingRids.end());

    if (matchingRids.size() > 1)
    {
        SWSS_LOG_WARN("ambiguous tunnel NH match: count=%zu tunnel_rid=%s type=%s, falling back to slow path",
                matchingRids.size(),
                sai_serialize_object_id(tunnelRid).c_str(),
                nhType.c_str());

        return SAI_NULL_OBJECT_ID;
    }

    return matchingRids.front();
}

std::string BestCandidateFinder::resolveNhgMemberKeyViaIdentityIndex(
        _In_ sai_object_id_t tempNhgVid)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> nhRids;

    for (const auto &member: m_temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER))
    {
        if (!member->hasAttr(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID))
        {
            continue;
        }

        if (member->getSaiAttr(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID)->getOid() != tempNhgVid)
        {
            continue;
        }

        if (!member->hasAttr(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID))
        {
            return "";
        }

        sai_object_id_t nhVid = member->getSaiAttr(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID)->getOid();
        sai_object_id_t nhRid = resolveNextHopRidForNhgKey(nhVid);

        if (nhRid == SAI_NULL_OBJECT_ID)
        {
            return "";
        }

        nhRids.push_back(nhRid);
    }

    if (nhRids.empty())
    {
        return "";
    }

    std::sort(nhRids.begin(), nhRids.end());

    std::ostringstream key;

    for (size_t i = 0; i < nhRids.size(); i++)
    {
        if (i > 0)
        {
            key << ";";
        }

        key << sai_serialize_object_id(nhRids.at(i));
    }

    return key.str();
}

std::shared_ptr<SaiObj> BestCandidateFinder::findCurrentBestMatchForNextHopGroupUsingMemberIndex(
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    const std::string key = resolveNhgMemberKeyViaIdentityIndex(temporaryObj->getVid());

    if (key.empty())
    {
        return nullptr;
    }

    const auto *index = m_currentView.getNhgMemberIndex();
    const auto it = index->find(key);

    if (it == index->end())
    {
        return nullptr;
    }

    for (const auto &nhg: it->second)
    {
        if (nhg->getObjectStatus() != SAI_OBJECT_STATUS_NOT_PROCESSED)
        {
            continue;
        }

        if (it->second.size() > 1)
        {
            SWSS_LOG_NOTICE("NHG greedy member-key match for %s -> %s (pool size %zu)",
                    temporaryObj->m_str_object_id.c_str(),
                    nhg->m_str_object_id.c_str(),
                    it->second.size());
        }
        else
        {
            SWSS_LOG_NOTICE("NHG unique member-key match for %s -> %s",
                    temporaryObj->m_str_object_id.c_str(),
                    nhg->m_str_object_id.c_str());
        }

        return nhg;
    }

    return nullptr;
}
