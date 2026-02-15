#include "View.h"

#include "syncd/ComparisonLogic.h"
#include "syncd/VidManager.h"

#include "meta/sai_serialize.h"

#include "SaiSwitchAsic.h"

using json = nlohmann::json;

using namespace saiasiccmp;

View::View(
        _In_ const std::string& filename):
    m_maxObjectIndex(0),
    m_otherMaxObjectIndex(0)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("loading view from: %s", filename.c_str());

    std::ifstream file(filename);

    if (!file.good())
    {
        SWSS_LOG_THROW("failed to open %s", filename.c_str());
    }

    json j;
    file >> j;

    loadVidRidMaps(j);
    loadAsicView(j);
    loadColdVids(j);
    loadHidden(j);

    for (auto& it: m_objTypeStrMap)
    {
        SWSS_LOG_NOTICE("%s: %zu", sai_serialize_object_type(it.first).c_str(), it.second.size());
    }
}

void View::loadVidRidMaps(
        _In_ const json& j)
{
    SWSS_LOG_ENTER();

    json v2r = j["VIDTORID"].at("value");

    for (auto it = v2r.begin(); it != v2r.end(); it++)
    {
        std::string v = it.key();
        std::string r = it.value();

        sai_object_id_t vid;
        sai_object_id_t rid;

        sai_deserialize_object_id(v, vid);
        sai_deserialize_object_id(r, rid);

        m_vid2rid[vid] = rid;
        m_rid2vid[rid] = vid;

        auto ot = syncd::VidManager::objectTypeQuery(vid);

        m_oidTypeMap[ot].insert(vid);

        uint64_t index = syncd::VidManager::getObjectIndex(vid);

        m_maxObjectIndex = std::max(m_maxObjectIndex, index);
    }

    SWSS_LOG_NOTICE("oids: %zu\n", m_vid2rid.size());

    if (m_oidTypeMap.at(SAI_OBJECT_TYPE_SWITCH).size() != 1)
    {
        SWSS_LOG_THROW("expected only 1 switch in view, FIXME");
    }

    m_switchVid = *m_oidTypeMap.at(SAI_OBJECT_TYPE_SWITCH).begin();

    m_switchRid = m_vid2rid.at(m_switchVid);
}

void View::loadAsicView(
        _In_ const json& j)
{
    SWSS_LOG_ENTER();

    for (auto it = j.begin(); it != j.end(); it++)
    {
        std::string key = it.key();

        if (key.rfind("ASIC_STATE:") != 0)
            continue;

        json vals = it.value().at("value");

        // skip ASIC_STATE
        key = key.substr(key.find_first_of(":") + 1);

        sai_object_meta_key_t mk;
        sai_deserialize_object_meta_key(key, mk);

        m_objTypeStrMap[mk.objecttype].insert(key);

        m_dump[key] = {}; // in case of NULL

        for (auto itt = vals.begin(); itt != vals.end(); itt++)
        {
            if (itt.key() != "NULL")
            {
                m_dump[key][itt.key()] = itt.value();
            }
        }
    }

    m_asicView = std::make_shared<syncd::AsicView>(m_dump);

    SWSS_LOG_NOTICE("view objects: %zu", m_asicView->m_soAll.size());
}

void View::loadColdVids(
        _In_ const json& j)
{
    SWSS_LOG_ENTER();

    json cold = j["COLDVIDS"].at("value"); // TODO depend on switch

    for (auto it = cold.begin(); it != cold.end(); it++)
    {
        std::string v = it.key();
        std::string o = it.value();

        sai_object_id_t vid;
        sai_object_type_t ot;

        sai_deserialize_object_id(v, vid);
        sai_deserialize_object_type(o, ot);

        m_coldVids[vid] = ot;
    }

    SWSS_LOG_NOTICE("cold vids: %zu", m_coldVids.size());
}

void View::loadHidden(
        _In_ const json& j)
{
    SWSS_LOG_ENTER();

    json hidden = j["HIDDEN"].at("value"); // TODO depend on switch

    for (auto it = hidden.begin(); it != hidden.end(); it++)
    {
        std::string h = it.key();
        std::string r = it.value();

        sai_object_id_t rid;

        sai_deserialize_object_id(r, rid);

        m_hidden[h] = rid;
    }

    SWSS_LOG_NOTICE("hidden: %zu", m_hidden.size());
}

void View::translateViewVids(
        _In_ uint64_t otherMaxObjectIndex)
{
    SWSS_LOG_ENTER();

    // TODO use other views ?

    m_otherMaxObjectIndex = otherMaxObjectIndex;

    SWSS_LOG_NOTICE("max index: %lu (0x%lx), other max index: %lu (0x%lx)",
            m_maxObjectIndex,
            m_maxObjectIndex,
            m_otherMaxObjectIndex,
            m_otherMaxObjectIndex);

    translateVidRidMaps();
    translateAsicView();
    translateColdVids();

    // no need for hidden, since they are RIDs
}

void View::translateVidRidMaps()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("translating old VIDs to new VIDs");

    // TODO consider saving oids when rid/vid match both views

    m_oldVid2NewVid.clear();

    uint64_t index = std::max(m_maxObjectIndex, m_otherMaxObjectIndex) + 1;

    // translate except our starting point

    auto copy = m_vid2rid;

    m_vid2rid.clear();
    m_rid2vid.clear();

    m_oidTypeMap.clear();

    for (auto it: copy)
    {
        auto oldVid = it.first;
        auto rid = it.second;
        auto newVid = syncd::VidManager::updateObjectIndex(oldVid, index);

        index++;

        auto ot = syncd::VidManager::objectTypeQuery(oldVid);

        switch (ot)
        {
            case SAI_OBJECT_TYPE_SWITCH:
            case SAI_OBJECT_TYPE_PORT:
            case SAI_OBJECT_TYPE_QUEUE:
            case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
            case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:

                // don't translate starting point vids
                newVid = oldVid;
                break;

            default:

                SWSS_LOG_INFO("translating old VID %s to new VID %s",
                        sai_serialize_object_id(oldVid).c_str(),
                        sai_serialize_object_id(newVid).c_str());
                break;
        }

        m_oldVid2NewVid[oldVid] = newVid;

        m_vid2rid[newVid] = rid;
        m_rid2vid[rid] = newVid;

        m_oidTypeMap[ot].insert(newVid);
    }
}

sai_object_id_t View::translateOldVidToNewVid(
        _In_ sai_object_id_t oldVid) const
{
    SWSS_LOG_ENTER();

    if (oldVid == SAI_NULL_OBJECT_ID)
        return SAI_NULL_OBJECT_ID;

    auto it = m_oldVid2NewVid.find(oldVid);

    if (it == m_oldVid2NewVid.end())
    {
        SWSS_LOG_THROW("missing old vid %s", sai_serialize_object_id(oldVid).c_str());
    }

    SWSS_LOG_INFO("translating old vid %s", sai_serialize_object_id(oldVid).c_str());

    return m_oldVid2NewVid.at(oldVid);
}

void View::translateMetaKeyVids(
        _Inout_ sai_object_meta_key_t& mk) const
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(mk.objecttype);

    if (info->isobjectid)
    {
        mk.objectkey.key.object_id = translateOldVidToNewVid(mk.objectkey.key.object_id);

        return;
    }

    // non object id, translate structure oids

    for (size_t j = 0; j < info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = info->structmembers[j];

        if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            continue;
        }

        sai_object_id_t vid = m->getoid(&mk);

        vid = translateOldVidToNewVid(vid);

        m->setoid(&mk, vid);
    }
}

void View::translateAttrVids(
        _In_ const sai_attr_metadata_t* meta,
        _Inout_ sai_attribute_t& attr)
{
    SWSS_LOG_ENTER();

    uint32_t count = 0;
    sai_object_id_t *objectIdList = NULL;

    switch (meta->attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:

            count = 1;
            objectIdList = &attr.value.oid;

            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:

            count = attr.value.objlist.count;
            objectIdList = attr.value.objlist.list;

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:

            if (attr.value.aclfield.enable)
            {
                count = 1;
                objectIdList = &attr.value.aclfield.data.oid;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

            if (attr.value.aclfield.enable)
            {
                count = attr.value.aclfield.data.objlist.count;
                objectIdList = attr.value.aclfield.data.objlist.list;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:

            if (attr.value.aclaction.enable)
            {
                count = 1;
                objectIdList = &attr.value.aclaction.parameter.oid;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

            if (attr.value.aclaction.enable)
            {
                count = attr.value.aclaction.parameter.objlist.count;
                objectIdList = attr.value.aclaction.parameter.objlist.list;
            }

            break;

        default:

            if (meta->isoidattribute)
            {
                SWSS_LOG_THROW("attribute %s is oid attrubute but not handled", meta->attridname);
            }

            // Attribute not contain any object ids.

            break;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        objectIdList[i] = translateOldVidToNewVid(objectIdList[i]);
    }
}

void View::translateAsicView()
{
    SWSS_LOG_ENTER();

    m_objTypeStrMap.clear();

    swss::TableDump dump;

    for (auto it: m_dump)
    {
        std::string oldkey = it.first;

        sai_object_meta_key_t mk;
        sai_deserialize_object_meta_key(oldkey, mk);

        translateMetaKeyVids(mk);

        std::string key = sai_serialize_object_meta_key(mk);

        m_objTypeStrMap[mk.objecttype].insert(key);

        SWSS_LOG_INFO("translated %s to %s", oldkey.c_str(), key.c_str());

        dump[key] = {}; // in case of NULL

        // TODO translate oid attributes

        for (auto at: it.second)
        {
            auto attrId = at.first;
            auto attrOldVal = at.second;

            syncd::SaiAttr attr(attrId, attrOldVal);

            translateAttrVids(attr.getAttrMetadata(), *attr.getRWSaiAttr());

            auto attrVal = sai_serialize_attr_value(*attr.getAttrMetadata(), *attr.getRWSaiAttr());

            SWSS_LOG_INFO("translate %s: %s to %s", attrId.c_str(), attrOldVal.c_str(), attrVal.c_str());

            dump[key][attrId] = attrVal;
        }
    }

    m_asicView = std::make_shared<syncd::AsicView>(dump);

    SWSS_LOG_NOTICE("view objects: %zu", m_asicView->m_soAll.size());
}

void View::translateColdVids()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("translating cold VIDs");

    auto copy = m_coldVids;

    m_coldVids.clear();

    for (auto it: copy)
    {
        auto oldVid = it.first;
        auto ot = it.second;

        if (m_oldVid2NewVid.find(oldVid) == m_oldVid2NewVid.end())
        {
            // some cold vids may be missing from rid vid map
            // like vlan member or bridge port
            continue;
        }

        m_coldVids[ m_oldVid2NewVid.at(oldVid) ] = ot;
    }

    SWSS_LOG_NOTICE("cold vids: %zu", m_coldVids.size());
}
