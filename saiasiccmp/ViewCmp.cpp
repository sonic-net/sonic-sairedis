#include "ViewCmp.h"
#include "View.h"
#include "SaiSwitchAsic.h"

#include "syncd/ComparisonLogic.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

using namespace saiasiccmp;

ViewCmp::ViewCmp(
        _In_ std::shared_ptr<View> a,
        _In_ std::shared_ptr<View> b):
    m_va(a),
    m_vb(b)
{
    SWSS_LOG_ENTER();

    if (a->m_asicView->m_soAll.size() != b->m_asicView->m_soAll.size())
    {
        SWSS_LOG_WARN("different number of objects in views %zu vs %zu",
                a->m_asicView->m_soAll.size(),
                b->m_asicView->m_soAll.size());
    }

    checkStartingPoint();
    checkVidRidMaps();
    checkHidden();

    // since second view can be translated, and some objects could
    // been removed (vlan members, bridge ports)
    // checkColdVids();
}

void ViewCmp::checkColdVids()
{
    SWSS_LOG_ENTER();

    // both cold vids should be the same, except when translated

    if (m_va->m_coldVids.size() != m_vb->m_coldVids.size())
    {
        SWSS_LOG_THROW("cold vids sizes differ: %zu vs %zu",
                m_va->m_coldVids.size(),
                m_vb->m_coldVids.size());
    }

    for (auto it: m_va->m_coldVids)
    {
        if (m_vb->m_coldVids.find(it.first) == m_vb->m_coldVids.end())
        {
            SWSS_LOG_THROW("VID %s missing from second view", sai_serialize_object_id(it.first).c_str());
        }
    }
}

void ViewCmp::checkHidden()
{
    SWSS_LOG_ENTER();

    if (m_va->m_hidden.size() != m_vb->m_hidden.size())
    {
        SWSS_LOG_THROW("hidden size don't match");
    }

    for (auto it: m_va->m_hidden)
    {
        auto key = it.first;
        auto val = it.second;

        if (m_vb->m_hidden.find(key) == m_vb->m_hidden.end())
        {
            SWSS_LOG_THROW("second view missing hidden %s", key.c_str());
        }

        if (m_vb->m_hidden.at(key) != val)
        {
            SWSS_LOG_THROW("second view hidden %s value missmatch", key.c_str());
        }
    }
}

void ViewCmp::checkVidRidMaps()
{
    SWSS_LOG_ENTER();

    checkVidRidMaps(m_va, m_vb);
    checkVidRidMaps(m_vb, m_va);
}

void ViewCmp::checkVidRidMaps(
        _In_ std::shared_ptr<View> a,
        _In_ std::shared_ptr<View> b)
{
    SWSS_LOG_ENTER();

    for (auto it: a->m_vid2rid)
    {
        auto avid = it.first;
        auto arid = it.second;

        if (b->m_vid2rid.find(avid) != b->m_vid2rid.end() &&
                b->m_vid2rid.at(avid) != arid)
        {
            SWSS_LOG_THROW("vid %s exists, but have different rid value %s vs %s",
                    sai_serialize_object_id(avid).c_str(),
                    sai_serialize_object_id(arid).c_str(),
                    sai_serialize_object_id(b->m_vid2rid.at(avid)).c_str());
        }
    }
}

void ViewCmp::checkStartingPoint()
{
    SWSS_LOG_ENTER();

    // we assume at starting point vid/rid will match
    // on switches, ports, queues, scheduler groups, ipgs

    std::vector<sai_object_type_t> ot = {
        SAI_OBJECT_TYPE_SWITCH,
        SAI_OBJECT_TYPE_PORT,
        SAI_OBJECT_TYPE_QUEUE,
        SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
    };

    for (auto o: ot)
    {
        checkStartingPoint(o);
    }

    SWSS_LOG_NOTICE("starting point success");
}

void ViewCmp::checkStartingPoint(
        _In_ sai_object_type_t ot)
{
    SWSS_LOG_ENTER();

    if (m_va->m_oidTypeMap.size() != m_vb->m_oidTypeMap.size())
    {
        SWSS_LOG_THROW("different object %s count: %zu vs %zu",
                sai_serialize_object_type(ot).c_str(),
                m_va->m_oidTypeMap.size(),
                m_vb->m_oidTypeMap.size());
    }

    for (auto vid: m_va->m_oidTypeMap.at(ot))
    {
        auto it = m_vb->m_vid2rid.find(vid);

        if (it == m_vb->m_vid2rid.end())
        {
            SWSS_LOG_THROW("vid %s missing from second view",
                    sai_serialize_object_id(vid).c_str());
        }

        if (m_va->m_vid2rid.at(vid) != m_vb->m_vid2rid.at(vid))
        {
            SWSS_LOG_THROW("vid %s has different RID values: %s vs %s",
                    sai_serialize_object_id(vid).c_str(),
                    sai_serialize_object_id(m_va->m_vid2rid.at(vid)).c_str(),
                    sai_serialize_object_id(m_vb->m_vid2rid.at(vid)).c_str());
        }
    }
}

bool ViewCmp::compareViews(
        _In_ bool dumpDiffToStdErr)
{
    SWSS_LOG_ENTER();

    auto breakConfig = std::make_shared<syncd::BreakConfig>();

    std::set<sai_object_id_t> initViewRemovedVids;

    auto sw = std::make_shared<SaiSwitchAsic>(
            m_va->m_switchVid,
            m_va->m_switchRid,
            m_va->m_vid2rid,
            m_va->m_rid2vid,
            m_va->m_hidden,
            m_va->m_coldVids);

    auto cl = std::make_shared<syncd::ComparisonLogic>(
            nullptr, // m_vendorSai
            sw,
            nullptr, // handler
            initViewRemovedVids,
            m_va->m_asicView, // current
            m_vb->m_asicView, // temp
            breakConfig);

    cl->compareViews();

    // TODO support multiple asic views (multiple switch)

    if (m_va->m_asicView->asicGetOperationsCount())
    {
        SWSS_LOG_WARN("views are NOT EQUAL, operations count: %zu", m_va->m_asicView->asicGetOperationsCount());

        if (dumpDiffToStdErr)
        {
            std::cerr << "views are NOT EQUAL, operations count: " << m_va->m_asicView->asicGetOperationsCount() << std::endl;
        }

        for (const auto &op: m_va->m_asicView->asicGetOperations())
        {
            const std::string &key = kfvKey(*op.m_op);
            const std::string &opp = kfvOp(*op.m_op);

            SWSS_LOG_NOTICE("%s: %s", opp.c_str(), key.c_str());

            if (dumpDiffToStdErr)
            {
                std::cerr << opp << ": " << key << std::endl;
            }

            const auto &values = kfvFieldsValues(*op.m_op);

            for (auto &val: values)
            {
                SWSS_LOG_NOTICE("- %s %s", fvField(val).c_str(), fvValue(val).c_str());

                if (dumpDiffToStdErr)
                {
                    std::cerr << "- " << fvField(val) << " " << fvValue(val) << std::endl;
                }
            }
        }

        return false;
    }

    SWSS_LOG_NOTICE("views are equal");

    return true;
}
