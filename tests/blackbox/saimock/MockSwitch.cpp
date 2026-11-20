#include "MockSai.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <cstring>

using namespace saimock;

/*
 * The objects a switch has right after its creation, shaped like a typical
 * NPU: a CPU port, front panel ports with their queues, scheduler groups and
 * priority groups, the default virtual router, VLAN, 1Q bridge, STP
 * instance, trap group and hashes.
 */

static sai_attribute_t attrU32(
        _In_ sai_attr_id_t id,
        _In_ uint32_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.u32 = value;

    return attr;
}

static sai_attribute_t attrU16(
        _In_ sai_attr_id_t id,
        _In_ uint16_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.u16 = value;

    return attr;
}

static sai_attribute_t attrU8(
        _In_ sai_attr_id_t id,
        _In_ uint8_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.u8 = value;

    return attr;
}

static sai_attribute_t attrS32(
        _In_ sai_attr_id_t id,
        _In_ int32_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.s32 = value;

    return attr;
}

static sai_attribute_t attrBool(
        _In_ sai_attr_id_t id,
        _In_ bool value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.booldata = value;

    return attr;
}

static sai_attribute_t attrOid(
        _In_ sai_attr_id_t id,
        _In_ sai_object_id_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.oid = value;

    return attr;
}

static sai_attribute_t attrU32List(
        _In_ sai_attr_id_t id,
        _In_ std::vector<uint32_t>& values)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.u32list.count = (uint32_t)values.size();
    attr.value.u32list.list = values.data();

    return attr;
}

static sai_attribute_t attrS32List(
        _In_ sai_attr_id_t id,
        _In_ std::vector<int32_t>& values)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = id;
    attr.value.s32list.count = (uint32_t)values.size();
    attr.value.s32list.list = values.data();

    return attr;
}

void MockSai::createPortChildren(
        _In_ SwitchModel& model,
        _In_ sai_object_id_t portRid,
        _In_ bool cpu)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> queues;
    std::vector<sai_object_id_t> groups;
    std::vector<sai_object_id_t> pgs;

    uint32_t queueCount = cpu ? m_config.cpuQueues : m_config.queuesPerPort;

    // unicast and multicast queue of a traffic class share a scheduler group

    uint32_t classes = cpu ? 0 : (queueCount + 1) / 2;

    sai_object_id_t root = addObject(model, SAI_OBJECT_TYPE_SCHEDULER_GROUP, {
            attrOid(SAI_SCHEDULER_GROUP_ATTR_PORT_ID, portRid),
            attrU8(SAI_SCHEDULER_GROUP_ATTR_LEVEL, 0),
            attrU8(SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS, (uint8_t)std::min<uint32_t>(cpu ? queueCount : classes, 255)),
            attrOid(SAI_SCHEDULER_GROUP_ATTR_PARENT_NODE, portRid) });

    groups.push_back(root);

    std::vector<sai_object_id_t> classGroups;

    for (uint32_t tc = 0; tc < classes; tc++)
    {
        classGroups.push_back(addObject(model, SAI_OBJECT_TYPE_SCHEDULER_GROUP, {
                    attrOid(SAI_SCHEDULER_GROUP_ATTR_PORT_ID, portRid),
                    attrU8(SAI_SCHEDULER_GROUP_ATTR_LEVEL, 1),
                    attrU8(SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS, 2),
                    attrOid(SAI_SCHEDULER_GROUP_ATTR_PARENT_NODE, root) }));
    }

    groups.insert(groups.end(), classGroups.begin(), classGroups.end());

    std::vector<std::vector<sai_object_id_t>> classChildren(classes);

    for (uint32_t idx = 0; idx < queueCount; idx++)
    {
        int32_t type = cpu ? SAI_QUEUE_TYPE_ALL :
            (idx < queueCount / 2 ? SAI_QUEUE_TYPE_UNICAST : SAI_QUEUE_TYPE_MULTICAST);

        sai_object_id_t parent = cpu ? root : classGroups[idx % classes];

        sai_object_id_t queue = addObject(model, SAI_OBJECT_TYPE_QUEUE, {
                attrS32(SAI_QUEUE_ATTR_TYPE, type),
                attrU8(SAI_QUEUE_ATTR_INDEX, (uint8_t)idx),
                attrOid(SAI_QUEUE_ATTR_PORT, portRid),
                attrOid(SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE, parent) });

        queues.push_back(queue);

        if (!cpu)
        {
            classChildren[idx % classes].push_back(queue);
        }
    }

    if (cpu)
    {
        setOidList(root, SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST, queues);
        setU32(root, SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT, (uint32_t)queues.size());
    }
    else
    {
        setOidList(root, SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST, classGroups);
        setU32(root, SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT, (uint32_t)classGroups.size());

        for (uint32_t tc = 0; tc < classes; tc++)
        {
            setOidList(classGroups[tc], SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST, classChildren[tc]);
            setU32(classGroups[tc], SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT, (uint32_t)classChildren[tc].size());
        }

        for (uint32_t idx = 0; idx < m_config.pgsPerPort; idx++)
        {
            pgs.push_back(addObject(model, SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, {
                        attrOid(SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT, portRid),
                        attrU8(SAI_INGRESS_PRIORITY_GROUP_ATTR_INDEX, (uint8_t)idx) }));
        }
    }

    setU32(portRid, SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES, (uint32_t)queues.size());
    setOidList(portRid, SAI_PORT_ATTR_QOS_QUEUE_LIST, queues);
    setU32(portRid, SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS, (uint32_t)groups.size());
    setOidList(portRid, SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST, groups);
    setU32(portRid, SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS, (uint32_t)pgs.size());
    setOidList(portRid, SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST, pgs);

    if (!cpu)
    {
        MockObject& port = m_oids.at(portRid);

        bool hasOperStatus = false;

        for (const auto& stored: port.attrs)
        {
            hasOperStatus |= (stored.attr.id == SAI_PORT_ATTR_OPER_STATUS);
        }

        if (!hasOperStatus)
        {
            storeAttr(port, attrS32(SAI_PORT_ATTR_OPER_STATUS,
                        m_config.portsUp ? SAI_PORT_OPER_STATUS_UP : SAI_PORT_OPER_STATUS_DOWN));
        }
    }

    auto& children = model.portChildren[portRid];

    children.insert(children.end(), queues.begin(), queues.end());
    children.insert(children.end(), groups.begin(), groups.end());
    children.insert(children.end(), pgs.begin(), pgs.end());
}

sai_status_t MockSai::createSwitch(
        _Out_ sai_object_id_t* switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    uint32_t index = 0;

    while (m_switches.find(index) != m_switches.end())
    {
        index++;
    }

    if (index > 0xff)
    {
        return SAI_STATUS_INSUFFICIENT_RESOURCES;
    }

    SwitchModel& model = m_switches[index];

    model.index = index;
    model.switchRid = encodeOid(SAI_OBJECT_TYPE_SWITCH, index, 0);

    sai_object_id_t sw = model.switchRid;

    MockObject& swObject = m_oids[sw];

    swObject.objectType = SAI_OBJECT_TYPE_SWITCH;

    m_counts[(int32_t)SAI_OBJECT_TYPE_SWITCH]++;

    storeAttrs(swObject, attr_count, attr_list, true);

    bool hasMac = false;

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        auto md = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_list[idx].id);

        if (md && md->attrvaluetype == SAI_ATTR_VALUE_TYPE_POINTER)
        {
            model.notifications[attr_list[idx].id] = attr_list[idx].value.ptr;
        }

        hasMac |= (attr_list[idx].id == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS);
    }

    if (!hasMac)
    {
        sai_attribute_t attr;

        memset(&attr, 0, sizeof(attr));

        attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

        sai_deserialize_mac(m_config.srcMac, attr.value.mac);

        storeAttr(swObject, attr);
    }

    model.cpuPort = addObject(model, SAI_OBJECT_TYPE_PORT, {
            attrS32(SAI_PORT_ATTR_TYPE, SAI_PORT_TYPE_CPU),
            attrS32(SAI_PORT_ATTR_OPER_STATUS, SAI_PORT_OPER_STATUS_UP) });

    createPortChildren(model, model.cpuPort, true);

    std::vector<uint32_t> speeds = { m_config.portSpeed };

    for (uint32_t port = 0; port < m_config.portCount; port++)
    {
        std::vector<uint32_t> lanes;

        for (uint32_t lane = 0; lane < m_config.lanesPerPort; lane++)
        {
            lanes.push_back(port * m_config.lanesPerPort + lane);
        }

        sai_object_id_t portRid = addObject(model, SAI_OBJECT_TYPE_PORT, {
                attrS32(SAI_PORT_ATTR_TYPE, SAI_PORT_TYPE_LOGICAL),
                attrU32List(SAI_PORT_ATTR_HW_LANE_LIST, lanes),
                attrU32(SAI_PORT_ATTR_SPEED, m_config.portSpeed),
                attrU32List(SAI_PORT_ATTR_SUPPORTED_SPEED, speeds) });

        model.ports.push_back(portRid);

        createPortChildren(model, portRid, false);
    }

    sai_object_id_t vr = addObject(model, SAI_OBJECT_TYPE_VIRTUAL_ROUTER, {});

    sai_object_id_t bridge = addObject(model, SAI_OBJECT_TYPE_BRIDGE, {
            attrS32(SAI_BRIDGE_ATTR_TYPE, SAI_BRIDGE_TYPE_1Q) });

    if (m_config.defaultBridgePorts)
    {
        for (auto portRid: model.ports)
        {
            model.bridgePorts.push_back(addObject(model, SAI_OBJECT_TYPE_BRIDGE_PORT, {
                        attrS32(SAI_BRIDGE_PORT_ATTR_TYPE, SAI_BRIDGE_PORT_TYPE_PORT),
                        attrOid(SAI_BRIDGE_PORT_ATTR_PORT_ID, portRid),
                        attrBool(SAI_BRIDGE_PORT_ATTR_ADMIN_STATE, true) }));
        }
    }

    setOidList(bridge, SAI_BRIDGE_ATTR_PORT_LIST, model.bridgePorts);

    sai_object_id_t stp = addObject(model, SAI_OBJECT_TYPE_STP, {
            attrOid(SAI_STP_ATTR_BRIDGE_ID, bridge) });

    model.defaultVlan = addObject(model, SAI_OBJECT_TYPE_VLAN, {
            attrU16(SAI_VLAN_ATTR_VLAN_ID, 1),
            attrOid(SAI_VLAN_ATTR_STP_INSTANCE, stp) });

    std::vector<sai_object_id_t> members;

    if (m_config.defaultVlanMembers)
    {
        for (auto bridgePort: model.bridgePorts)
        {
            members.push_back(addObject(model, SAI_OBJECT_TYPE_VLAN_MEMBER, {
                        attrOid(SAI_VLAN_MEMBER_ATTR_VLAN_ID, model.defaultVlan),
                        attrOid(SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID, bridgePort),
                        attrS32(SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE, SAI_VLAN_TAGGING_MODE_UNTAGGED) }));
        }
    }

    setOidList(model.defaultVlan, SAI_VLAN_ATTR_MEMBER_LIST, members);

    sai_object_id_t trapGroup = addObject(model, SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, {
            attrBool(SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE, true),
            attrU32(SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE, 0) });

    std::vector<int32_t> fields = {
        SAI_NATIVE_HASH_FIELD_SRC_IP,
        SAI_NATIVE_HASH_FIELD_DST_IP,
        SAI_NATIVE_HASH_FIELD_L4_SRC_PORT,
        SAI_NATIVE_HASH_FIELD_L4_DST_PORT,
        SAI_NATIVE_HASH_FIELD_IP_PROTOCOL };

    sai_object_id_t ecmpHash = addObject(model, SAI_OBJECT_TYPE_HASH, {
            attrS32List(SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST, fields) });

    sai_object_id_t lagHash = addObject(model, SAI_OBJECT_TYPE_HASH, {
            attrS32List(SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST, fields) });

    setOid(sw, SAI_SWITCH_ATTR_CPU_PORT, model.cpuPort);
    setOid(sw, SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID, vr);
    setOid(sw, SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID, bridge);
    setOid(sw, SAI_SWITCH_ATTR_DEFAULT_VLAN_ID, model.defaultVlan);
    setOid(sw, SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID, stp);
    setOid(sw, SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP, trapGroup);
    setOid(sw, SAI_SWITCH_ATTR_ECMP_HASH, ecmpHash);
    setOid(sw, SAI_SWITCH_ATTR_LAG_HASH, lagHash);

    setU32(sw, SAI_SWITCH_ATTR_MAX_NUMBER_OF_SUPPORTED_PORTS, m_config.portCount);
    setU32(sw, SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES, m_config.queuesPerPort / 2);
    setU32(sw, SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES, m_config.queuesPerPort - m_config.queuesPerPort / 2);
    setU32(sw, SAI_SWITCH_ATTR_NUMBER_OF_QUEUES, m_config.queuesPerPort);
    setU32(sw, SAI_SWITCH_ATTR_NUMBER_OF_CPU_QUEUES, m_config.cpuQueues);
    setU32(sw, SAI_SWITCH_ATTR_PORT_MAX_MTU, 9216);
    setU32(sw, SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT, 64);
    setU32(sw, SAI_SWITCH_ATTR_ECMP_MEMBERS, 16384);
    setU32(sw, SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS, 4096);

    *switchId = sw;

    SWSS_LOG_NOTICE("created switch %s: %zu ports, %" PRIu64 " objects",
            sai_serialize_object_id(sw).c_str(),
            model.ports.size(),
            (uint64_t)m_oids.size());

    return SAI_STATUS_SUCCESS;
}

/*
 * The switch of an entry, as a route or an FDB entry: every entry struct has
 * a switch_id.
 */
static sai_object_id_t entrySwitchId(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    for (size_t idx = 0; info && idx < info->structmemberscount; idx++)
    {
        auto member = info->structmembers[idx];

        if (member->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID && strcmp(member->membername, "switch_id") == 0)
        {
            return member->getoid(&metaKey);
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void MockSai::removeSwitch(
        _In_ uint32_t index)
{
    SWSS_LOG_ENTER();

    for (auto it = m_oids.begin(); it != m_oids.end();)
    {
        if (switchIndexOf(it->first) == index)
        {
            m_counts[(int32_t)it->second.objectType]--;

            it = m_oids.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = m_entries.begin(); it != m_entries.end();)
    {
        if (switchIndexOf(entrySwitchId(it->first)) == index)
        {
            m_counts[(int32_t)it->first.objecttype]--;

            it = m_entries.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_switches.erase(index);

    // and entries of no known switch, with the last one

    if (m_switches.empty())
    {
        for (const auto& entry: m_entries)
        {
            m_counts[(int32_t)entry.first.objecttype]--;
        }

        m_entries.clear();
    }
}
