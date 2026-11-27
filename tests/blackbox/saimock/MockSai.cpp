#include "MockSai.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <algorithm>
#include <cstring>
#include <time.h>

#define MUTEX() std::lock_guard<std::mutex> _lock(m_mutex)

using namespace saimock;

/*
 * Real object id layout: object type in bits 63..48, switch index in bits
 * 47..40, object index in bits 39..0.
 */

#define MOCK_OID_TYPE_SHIFT     48
#define MOCK_OID_SWITCH_SHIFT   40
#define MOCK_OID_SWITCH_MASK    0xffULL
#define MOCK_OID_INDEX_MASK     ((1ULL << MOCK_OID_SWITCH_SHIFT) - 1)

/*
 * Counter values grow by this much per call, so every poll reads new values.
 */

#define MOCK_COUNTER_STEP       1000

static const char* const g_opNames[] = {
    "create",
    "remove",
    "set",
    "get",
    "bulk_create",
    "bulk_remove",
    "bulk_set",
    "bulk_get",
    "get_stats",
    "get_stats_ext",
    "clear_stats",
    "bulk_get_stats",
    "bulk_clear_stats",
    "object_type_query",
    "switch_id_query",
    "query",
    "flush_fdb",
    "notify",
    "*",
};

const char* saimock::opName(
        _In_ Op op)
{
    SWSS_LOG_ENTER();

    auto index = static_cast<size_t>(op);

    if (index < sizeof(g_opNames) / sizeof(g_opNames[0]))
    {
        return g_opNames[index];
    }

    return "unknown";
}

bool saimock::opFromName(
        _In_ const std::string& name,
        _Out_ Op& op)
{
    SWSS_LOG_ENTER();

    for (size_t index = 0; index < sizeof(g_opNames) / sizeof(g_opNames[0]); index++)
    {
        if (name == g_opNames[index])
        {
            op = static_cast<Op>(index);

            return true;
        }
    }

    op = Op::ANY;

    return false;
}

CallScope::CallScope(
        _In_ MockSai& sai,
        _In_ Op op,
        _In_ sai_object_type_t objectType,
        _In_ uint64_t objects):
    m_sai(sai),
    m_op(op),
    m_objectType(objectType),
    m_objects(objects),
    m_errors(0),
    m_unsupported(0),
    m_startNs(MockSai::nowNs())
{
    SWSS_LOG_ENTER();

    // empty
}

CallScope::~CallScope()
{
    SWSS_LOG_ENTER();

    uint64_t delayNs = 0;

    // an SDK refuses what it does not support at once

    if (m_sai.m_hasDelays && !m_unsupported)
    {
        bool sleep = false;

        delayNs = m_sai.delayFor(m_op, m_objectType, m_objects, sleep);

        if (delayNs && sleep)
        {
            struct timespec ts;

            ts.tv_sec = (time_t)(delayNs / 1000000000ULL);
            ts.tv_nsec = (long)(delayNs % 1000000000ULL);

            while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
            {
                // continue sleeping for the remaining time
            }
        }
        else if (delayNs)
        {
            uint64_t until = MockSai::nowNs() + delayNs;

            while (MockSai::nowNs() < until)
            {
                // spin, like an SDK busy with the hardware
            }
        }
    }

    m_sai.account(m_op, m_objectType, m_objects, m_errors, m_unsupported, m_startNs, delayNs, MockSai::nowNs());
}

sai_status_t CallScope::done(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // not supported answers what the SDK lacks, buffer overflow a size query:
    // neither is a failure

    if (status == SAI_STATUS_NOT_SUPPORTED || status == SAI_STATUS_NOT_IMPLEMENTED)
    {
        m_unsupported = 1;
    }
    else if (status != SAI_STATUS_SUCCESS && status != SAI_STATUS_BUFFER_OVERFLOW)
    {
        m_errors = 1;
    }

    return status;
}

sai_status_t CallScope::done(
        _In_ sai_status_t status,
        _In_ uint32_t count,
        _In_ const sai_status_t* statuses)
{
    SWSS_LOG_ENTER();

    m_errors = 0;

    if (status == SAI_STATUS_NOT_SUPPORTED || status == SAI_STATUS_NOT_IMPLEMENTED)
    {
        m_unsupported = 1;

        return status;
    }

    if (statuses)
    {
        for (uint32_t idx = 0; idx < count; idx++)
        {
            if (statuses[idx] != SAI_STATUS_SUCCESS)
            {
                m_errors++;
            }
        }
    }

    if (status != SAI_STATUS_SUCCESS && m_errors == 0)
    {
        m_errors = 1;
    }

    return status;
}

MockSai::MockSai():
    m_apiInitialized(false),
    m_nextIndex(1),
    m_statsTick(0),
    m_statsSinceNs(nowNs()),
    m_hasDelays(false),
    m_runControl(false),
    m_controlFd(-1),
    m_stopFd(-1),
    m_notifiersRunning(0)
{
    SWSS_LOG_ENTER();

    // empty
}

MockSai::~MockSai()
{
    SWSS_LOG_ENTER();

    stopControlServer();

    joinNotifiers();
}

uint64_t MockSai::nowNs()
{
    SWSS_LOG_ENTER();

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t MockSai::delayFor(
        _In_ Op op,
        _In_ sai_object_type_t objectType,
        _In_ uint64_t objects,
        _Out_ bool& sleep) const
{
    SWSS_LOG_ENTER();

    sleep = false;

    const std::pair<Op, int32_t> keys[] = {
        { op, (int32_t)objectType },
        { op, -1 },
        { Op::ANY, (int32_t)objectType },
        { Op::ANY, -1 },
    };

    for (const auto& key: keys)
    {
        auto it = m_delays.find(key);

        if (it != m_delays.end())
        {
            sleep = it->second.sleep;

            return it->second.perCallNs + it->second.perObjectNs * objects;
        }
    }

    return 0;
}

void MockSai::account(
        _In_ Op op,
        _In_ sai_object_type_t objectType,
        _In_ uint64_t objects,
        _In_ uint64_t errors,
        _In_ uint64_t unsupported,
        _In_ uint64_t startNs,
        _In_ uint64_t delayNs,
        _In_ uint64_t endNs)
{
    SWSS_LOG_ENTER();

    auto& stat = m_stats[std::make_pair(op, (int32_t)objectType)];

    stat.calls++;
    stat.objects += objects;
    stat.errors += errors;
    stat.unsupported += unsupported;
    stat.sdkNs += endNs - startNs;
    stat.delayNs += delayNs;

    if (stat.firstNs == 0)
    {
        stat.firstNs = startNs;
    }

    stat.lastNs = endNs;
    stat.maxObjects = std::max(stat.maxObjects, objects);
}

sai_object_id_t MockSai::encodeOid(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t switchIndex,
        _In_ uint64_t index)
{
    SWSS_LOG_ENTER();

    return ((uint64_t)(uint32_t)objectType << MOCK_OID_TYPE_SHIFT) |
        (((uint64_t)switchIndex & MOCK_OID_SWITCH_MASK) << MOCK_OID_SWITCH_SHIFT) |
        (index & MOCK_OID_INDEX_MASK);
}

uint32_t MockSai::switchIndexOf(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    return (uint32_t)((oid >> MOCK_OID_SWITCH_SHIFT) & MOCK_OID_SWITCH_MASK);
}

sai_object_id_t MockSai::allocateOid(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t switchIndex)
{
    SWSS_LOG_ENTER();

    return encodeOid(objectType, switchIndex, m_nextIndex++);
}

SwitchModel* MockSai::modelOf(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return m_switches.empty() ? nullptr : &m_switches.begin()->second;
    }

    auto it = m_switches.find(switchIndexOf(oid));

    return it == m_switches.end() ? nullptr : &it->second;
}

void MockSai::storeAttr(
        _Inout_ MockObject& object,
        _In_ const sai_attribute_t& attr)
{
    SWSS_LOG_ENTER();

    auto md = sai_metadata_get_attr_metadata(object.objectType, attr.id);

    if (md == nullptr)
    {
        SWSS_LOG_INFO("no metadata for %s attribute %d, not stored",
                sai_serialize_object_type(object.objectType).c_str(), attr.id);

        return;
    }

    StoredAttr stored;

    stored.attr = attr;

    if (!md->isprimitive)
    {
        try
        {
            stored.wrapper = std::make_shared<saimeta::SaiAttrWrapper>(md, attr);
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_WARN("failed to copy %s: %s, not stored", md->attridname, e.what());

            return;
        }
    }

    for (auto& existing: object.attrs)
    {
        if (existing.attr.id == attr.id)
        {
            existing = std::move(stored);

            return;
        }
    }

    object.attrs.push_back(std::move(stored));
}

void MockSai::storeAttrs(
        _Inout_ MockObject& object,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool force)
{
    SWSS_LOG_ENTER();

    if (!force && !m_config.storeAttributes)
    {
        return;
    }

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        storeAttr(object, attr_list[idx]);
    }
}

sai_object_id_t MockSai::addObject(
        _In_ SwitchModel& model,
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<sai_attribute_t>& attrs)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid = allocateOid(objectType, model.index);

    MockObject& object = m_oids[oid];

    object.objectType = objectType;

    storeAttrs(object, (uint32_t)attrs.size(), attrs.data(), true);

    m_counts[(int32_t)objectType]++;

    return oid;
}

void MockSai::setOidList(
        _In_ sai_object_id_t oid,
        _In_ sai_attr_id_t attrId,
        _In_ const std::vector<sai_object_id_t>& list)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> copy = list;

    sai_attribute_t attr;

    attr.id = attrId;
    attr.value.objlist.count = (uint32_t)copy.size();
    attr.value.objlist.list = copy.data();

    storeAttr(m_oids.at(oid), attr);
}

void MockSai::setU32(
        _In_ sai_object_id_t oid,
        _In_ sai_attr_id_t attrId,
        _In_ uint32_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = attrId;
    attr.value.u32 = value;

    storeAttr(m_oids.at(oid), attr);
}

void MockSai::setOid(
        _In_ sai_object_id_t oid,
        _In_ sai_attr_id_t attrId,
        _In_ sai_object_id_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = attrId;
    attr.value.oid = value;

    storeAttr(m_oids.at(oid), attr);
}

void MockSai::eraseOid(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto it = m_oids.find(oid);

    if (it == m_oids.end())
    {
        return;
    }

    m_counts[(int32_t)it->second.objectType]--;

    m_oids.erase(it);
}

bool MockSai::dynamicAttr(
        _In_ const MockObject& object,
        _In_ sai_object_id_t oid,
        _Inout_ sai_attribute_t& attr,
        _Out_ sai_status_t& status)
{
    SWSS_LOG_ENTER();

    status = SAI_STATUS_SUCCESS;

    if (object.objectType != SAI_OBJECT_TYPE_SWITCH)
    {
        return false;
    }

    SwitchModel* model = modelOf(oid);

    if (model == nullptr)
    {
        return false;
    }

    sai_attribute_t src;

    src.id = attr.id;

    switch (attr.id)
    {
        case SAI_SWITCH_ATTR_PORT_LIST:

            src.value.objlist.count = (uint32_t)model->ports.size();
            src.value.objlist.list = model->ports.data();
            break;

        case SAI_SWITCH_ATTR_PORT_NUMBER: // also SAI_SWITCH_ATTR_NUMBER_OF_ACTIVE_PORTS

            src.value.u32 = (uint32_t)model->ports.size();
            break;

        default:
            {
                auto md = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

                // resources left, as CRM asks for them

                if (md && md->attrvaluetype == SAI_ATTR_VALUE_TYPE_UINT32 &&
                        strncmp(md->attridname, "SAI_SWITCH_ATTR_AVAILABLE_", strlen("SAI_SWITCH_ATTR_AVAILABLE_")) == 0)
                {
                    src.value.u32 = 1000000;
                    break;
                }
            }

            return false;
    }

    status = transfer_attributes(SAI_OBJECT_TYPE_SWITCH, 1, &src, &attr, false);

    return true;
}

sai_status_t MockSai::defaultAttr(
        _In_ const MockObject& object,
        _In_ sai_object_id_t oid,
        _In_ const sai_attr_metadata_t& md,
        _Inout_ sai_attribute_t& attr)
{
    SWSS_LOG_ENTER();

    sai_attribute_t src;

    memset(&src, 0, sizeof(src));

    src.id = attr.id;

    if (md.defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_CONST && md.defaultvalue && md.isprimitive)
    {
        src.value = *md.defaultvalue;
    }
    else if (md.defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE &&
            md.defaultvalueobjecttype == SAI_OBJECT_TYPE_SWITCH && md.isprimitive)
    {
        // value of a switch attribute, like the router MAC of a virtual router

        SwitchModel* model = modelOf(oid);

        if (model)
        {
            auto it = m_oids.find(model->switchRid);

            if (it != m_oids.end())
            {
                for (const auto& stored: it->second.attrs)
                {
                    if (stored.attr.id == md.defaultvalueattrid)
                    {
                        src.value = stored.get()->value;
                        break;
                    }
                }
            }
        }
    }

    // otherwise zero: NULL object id, empty list, 0

    return transfer_attributes(object.objectType, 1, &src, &attr, false);
}

sai_status_t MockSai::getAttr(
        _In_ const MockObject& object,
        _In_ sai_object_id_t oid,
        _Inout_ sai_attribute_t& attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    if (dynamicAttr(object, oid, attr, status))
    {
        return status;
    }

    for (const auto& stored: object.attrs)
    {
        if (stored.attr.id == attr.id)
        {
            return transfer_attributes(object.objectType, 1, stored.get(), &attr, false);
        }
    }

    auto md = sai_metadata_get_attr_metadata(object.objectType, attr.id);

    if (md == nullptr)
    {
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return defaultAttr(object, oid, *md, attr);
}

sai_status_t MockSai::getAttrs(
        _In_ const MockObject& object,
        _In_ sai_object_id_t oid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_count && attr_list == nullptr)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t final = SAI_STATUS_SUCCESS;

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        sai_status_t status;

        try
        {
            status = getAttr(object, oid, attr_list[idx]);
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("get %s attribute %d failed: %s",
                    sai_serialize_object_type(object.objectType).c_str(),
                    attr_list[idx].id,
                    e.what());

            status = SAI_STATUS_FAILURE;
        }

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            final = status;
        }
        else if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
    }

    return final;
}

sai_status_t MockSai::apiInitialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("api already initialized");

        return SAI_STATUS_FAILURE;
    }

    loadConfig(service_method_table);

    m_apiInitialized = true;

    startControlServer();

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();

    // the control server and notifiers take the mutex, stop them first

    stopControlServer();

    joinNotifiers();

    MUTEX();

    m_oids.clear();
    m_entries.clear();
    m_counts.clear();
    m_switches.clear();

    m_apiInitialized = false;

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doCreate(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (objectId == nullptr || (attr_count && attr_list == nullptr))
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        return createSwitch(objectId, attr_count, attr_list);
    }

    auto info = sai_metadata_get_object_type_info(objectType);

    if (info == nullptr || !info->isobjectid)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    SwitchModel* model = modelOf(switchId);

    if (model == nullptr || switchId == SAI_NULL_OBJECT_ID)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t oid = allocateOid(objectType, model->index);

    MockObject& object = m_oids[oid];

    object.objectType = objectType;

    // syncd reads the lanes of a port it created

    storeAttrs(object, attr_count, attr_list, objectType == SAI_OBJECT_TYPE_PORT);

    m_counts[(int32_t)objectType]++;

    if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        model->ports.push_back(oid);

        createPortChildren(*model, oid, false);
    }

    *objectId = oid;

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doRemove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto it = m_oids.find(objectId);

    if (it == m_oids.end() || it->second.objectType != objectType)
    {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        removeSwitch(switchIndexOf(objectId));

        return SAI_STATUS_SUCCESS;
    }

    if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        SwitchModel* model = modelOf(objectId);

        if (model)
        {
            model->ports.erase(std::remove(model->ports.begin(), model->ports.end(), objectId), model->ports.end());

            for (auto child: model->portChildren[objectId])
            {
                eraseOid(child);
            }

            model->portChildren.erase(objectId);
        }
    }

    eraseOid(objectId);

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doSet(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto it = m_oids.find(objectId);

    if (it == m_oids.end() || it->second.objectType != objectType)
    {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        auto md = sai_metadata_get_attr_metadata(objectType, attr->id);

        SwitchModel* model = modelOf(objectId);

        if (md && model && md->attrvaluetype == SAI_ATTR_VALUE_TYPE_POINTER)
        {
            model->notifications[attr->id] = attr->value.ptr;
        }
    }

    storeAttrs(it->second, 1, attr, objectType == SAI_OBJECT_TYPE_SWITCH || objectType == SAI_OBJECT_TYPE_PORT);

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doGet(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto it = m_oids.find(objectId);

    if (it == m_oids.end() || it->second.objectType != objectType)
    {
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    return getAttrs(it->second, objectId, attr_count, attr_list);
}

sai_status_t MockSai::doCreateEntry(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_count && attr_list == nullptr)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto result = m_entries.emplace(metaKey, MockObject());

    if (!result.second)
    {
        return SAI_STATUS_ITEM_ALREADY_EXISTS;
    }

    MockObject& object = result.first->second;

    object.objectType = metaKey.objecttype;

    storeAttrs(object, attr_count, attr_list, false);

    m_counts[(int32_t)metaKey.objecttype]++;

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doRemoveEntry(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();

    auto it = m_entries.find(metaKey);

    if (it == m_entries.end())
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    m_entries.erase(it);

    m_counts[(int32_t)metaKey.objecttype]--;

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doSetEntry(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto it = m_entries.find(metaKey);

    if (it == m_entries.end())
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    storeAttrs(it->second, 1, attr, false);

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSai::doGetEntry(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto it = m_entries.find(metaKey);

    if (it == m_entries.end())
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    return getAttrs(it->second, SAI_NULL_OBJECT_ID, attr_count, attr_list);
}

sai_status_t MockSai::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::CREATE, objectType, 1);

    return scope.done(doCreate(objectType, objectId, switchId, attr_count, attr_list));
}

sai_status_t MockSai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::REMOVE, objectType, 1);

    return scope.done(doRemove(objectType, objectId));
}

sai_status_t MockSai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::SET, objectType, 1);

    return scope.done(doSet(objectType, objectId, attr));
}

sai_status_t MockSai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::GET, objectType, 1);

    return scope.done(doGet(objectType, objectId, attr_count, attr_list));
}

sai_status_t MockSai::entryCreate(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::CREATE, metaKey.objecttype, 1);

    return scope.done(doCreateEntry(metaKey, attr_count, attr_list));
}

sai_status_t MockSai::entryRemove(
        _In_ const sai_object_meta_key_t& metaKey)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::REMOVE, metaKey.objecttype, 1);

    return scope.done(doRemoveEntry(metaKey));
}

sai_status_t MockSai::entrySet(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::SET, metaKey.objecttype, 1);

    return scope.done(doSetEntry(metaKey, attr));
}

sai_status_t MockSai::entryGet(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::GET, metaKey.objecttype, 1);

    return scope.done(doGetEntry(metaKey, attr_count, attr_list));
}

/*
 * Bulk operations go one object after another, like a vendor without
 * hardware batching would, stopping at the first error when asked to.
 */

#define MOCK_BULK_LOOP(statuses, count, mode, op)                       \
    bool failed = false;                                                \
    for (uint32_t idx = 0; idx < (count); idx++)                        \
    {                                                                   \
        if (failed && (mode) == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)   \
        {                                                               \
            (statuses)[idx] = SAI_STATUS_NOT_EXECUTED;                  \
            continue;                                                   \
        }                                                               \
        (statuses)[idx] = (op);                                         \
        failed |= ((statuses)[idx] != SAI_STATUS_SUCCESS);              \
    }                                                                   \
    sai_status_t all = failed ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;

sai_status_t MockSai::entryBulkCreate(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t object_count,
        _In_ const sai_object_meta_key_t *metaKeys,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_CREATE, objectType, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!metaKeys || !attr_count || !attr_list || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doCreateEntry(metaKeys[idx], attr_count[idx], attr_list[idx]));

    return scope.done(all, object_count, object_statuses);
}

sai_status_t MockSai::entryBulkRemove(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t object_count,
        _In_ const sai_object_meta_key_t *metaKeys,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_REMOVE, objectType, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!metaKeys || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doRemoveEntry(metaKeys[idx]));

    return scope.done(all, object_count, object_statuses);
}

sai_status_t MockSai::entryBulkSet(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t object_count,
        _In_ const sai_object_meta_key_t *metaKeys,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_SET, objectType, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!metaKeys || !attr_list || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doSetEntry(metaKeys[idx], &attr_list[idx]));

    return scope.done(all, object_count, object_statuses);
}

sai_status_t MockSai::entryBulkGet(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t object_count,
        _In_ const sai_object_meta_key_t *metaKeys,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_GET, objectType, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!metaKeys || !attr_count || !attr_list || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doGetEntry(metaKeys[idx], attr_count[idx], attr_list[idx]));

    return scope.done(all, object_count, object_statuses);
}

/*
 * Every entry type gets the same generic implementation through its meta key.
 */

#define MOCK_META_KEY(OT, ot, entry)                                    \
    sai_object_meta_key_t mk;                                           \
    memset(&mk, 0, sizeof(mk));                                         \
    mk.objecttype = (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT;          \
    mk.objectkey.key.ot = *(entry);

#define MOCK_DEFINE_QUAD_ENTRY(OT, ot)                                  \
sai_status_t MockSai::create(                                           \
        _In_ const sai_ ## ot ## _t* ot,                                \
        _In_ uint32_t attr_count,                                       \
        _In_ const sai_attribute_t *attr_list)                          \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    if (ot == nullptr) return SAI_STATUS_INVALID_PARAMETER;             \
    MOCK_META_KEY(OT, ot, ot);                                          \
    return entryCreate(mk, attr_count, attr_list);                      \
}                                                                       \
sai_status_t MockSai::remove(                                           \
        _In_ const sai_ ## ot ## _t* ot)                                \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    if (ot == nullptr) return SAI_STATUS_INVALID_PARAMETER;             \
    MOCK_META_KEY(OT, ot, ot);                                          \
    return entryRemove(mk);                                             \
}                                                                       \
sai_status_t MockSai::set(                                              \
        _In_ const sai_ ## ot ## _t* ot,                                \
        _In_ const sai_attribute_t *attr)                               \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    if (ot == nullptr) return SAI_STATUS_INVALID_PARAMETER;             \
    MOCK_META_KEY(OT, ot, ot);                                          \
    return entrySet(mk, attr);                                          \
}                                                                       \
sai_status_t MockSai::get(                                              \
        _In_ const sai_ ## ot ## _t* ot,                                \
        _In_ uint32_t attr_count,                                       \
        _Out_ sai_attribute_t *attr_list)                               \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    if (ot == nullptr) return SAI_STATUS_INVALID_PARAMETER;             \
    MOCK_META_KEY(OT, ot, ot);                                          \
    return entryGet(mk, attr_count, attr_list);                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(MOCK_DEFINE_QUAD_ENTRY);

#define MOCK_META_KEYS(OT, ot, count)                                   \
    if ((count) && ot == nullptr) return SAI_STATUS_INVALID_PARAMETER;  \
    std::vector<sai_object_meta_key_t> keys(count);                     \
    for (uint32_t i = 0; i < (count); i++)                              \
    {                                                                   \
        keys[i].objecttype = (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT; \
        keys[i].objectkey.key.ot = ot[i];                               \
    }

#define MOCK_DEFINE_BULK_ENTRY(OT, ot)                                  \
sai_status_t MockSai::bulkCreate(                                       \
        _In_ uint32_t object_count,                                     \
        _In_ const sai_ ## ot ## _t *ot,                                \
        _In_ const uint32_t *attr_count,                                \
        _In_ const sai_attribute_t **attr_list,                         \
        _In_ sai_bulk_op_error_mode_t mode,                             \
        _Out_ sai_status_t *object_statuses)                            \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    MOCK_META_KEYS(OT, ot, object_count);                               \
    return entryBulkCreate((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,   \
            object_count, keys.data(), attr_count, attr_list,           \
            mode, object_statuses);                                     \
}                                                                       \
sai_status_t MockSai::bulkRemove(                                       \
        _In_ uint32_t object_count,                                     \
        _In_ const sai_ ## ot ## _t *ot,                                \
        _In_ sai_bulk_op_error_mode_t mode,                             \
        _Out_ sai_status_t *object_statuses)                            \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    MOCK_META_KEYS(OT, ot, object_count);                               \
    return entryBulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,   \
            object_count, keys.data(), mode, object_statuses);          \
}                                                                       \
sai_status_t MockSai::bulkSet(                                          \
        _In_ uint32_t object_count,                                     \
        _In_ const sai_ ## ot ## _t *ot,                                \
        _In_ const sai_attribute_t *attr_list,                          \
        _In_ sai_bulk_op_error_mode_t mode,                             \
        _Out_ sai_status_t *object_statuses)                            \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    MOCK_META_KEYS(OT, ot, object_count);                               \
    return entryBulkSet((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,      \
            object_count, keys.data(), attr_list, mode,                 \
            object_statuses);                                           \
}                                                                       \
sai_status_t MockSai::bulkGet(                                          \
        _In_ uint32_t object_count,                                     \
        _In_ const sai_ ## ot ## _t *ot,                                \
        _In_ const uint32_t *attr_count,                                \
        _Inout_ sai_attribute_t **attr_list,                            \
        _In_ sai_bulk_op_error_mode_t mode,                             \
        _Out_ sai_status_t *object_statuses)                            \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    MOCK_META_KEYS(OT, ot, object_count);                               \
    return entryBulkGet((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,      \
            object_count, keys.data(), attr_count, attr_list, mode,     \
            object_statuses);                                           \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(MOCK_DEFINE_BULK_ENTRY);

sai_status_t MockSai::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_CREATE, object_type, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!attr_count || !attr_list || !object_id || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doCreate(object_type, &object_id[idx], switch_id, attr_count[idx], attr_list[idx]));

    return scope.done(all, object_count, object_statuses);
}

sai_status_t MockSai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_REMOVE, object_type, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!object_id || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doRemove(object_type, object_id[idx]));

    return scope.done(all, object_count, object_statuses);
}

sai_status_t MockSai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_SET, object_type, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!object_id || !attr_list || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doSet(object_type, object_id[idx], &attr_list[idx]));

    return scope.done(all, object_count, object_statuses);
}

sai_status_t MockSai::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_GET, object_type, object_count);

    if (!m_config.bulkSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!object_id || !attr_count || !attr_list || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    MOCK_BULK_LOOP(object_statuses, object_count, mode, doGet(object_type, object_id[idx], attr_count[idx], attr_list[idx]));

    return scope.done(all, object_count, object_statuses);
}

void MockSai::fillCounters(
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    uint64_t base = ++m_statsTick * MOCK_COUNTER_STEP;

    for (uint32_t idx = 0; idx < number_of_counters; idx++)
    {
        counters[idx] = base + counter_ids[idx];
    }
}

sai_status_t MockSai::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::GET_STATS, object_type, 1);

    if (number_of_counters && (!counter_ids || !counters))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    if (m_oids.find(object_id) == m_oids.end())
    {
        return scope.done(SAI_STATUS_INVALID_OBJECT_ID);
    }

    fillCounters(number_of_counters, counter_ids, counters);

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::GET_STATS_EXT, object_type, 1);

    if (number_of_counters && (!counter_ids || !counters))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    if (m_oids.find(object_id) == m_oids.end())
    {
        return scope.done(SAI_STATUS_INVALID_OBJECT_ID);
    }

    fillCounters(number_of_counters, counter_ids, counters);

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::CLEAR_STATS, object_type, 1);

    if (m_oids.find(object_id) == m_oids.end())
    {
        return scope.done(SAI_STATUS_INVALID_OBJECT_ID);
    }

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::bulkGetStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_GET_STATS, object_type, object_count);

    if (!m_config.bulkStatsSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!object_key || !object_statuses || !counters || (number_of_counters && !counter_ids)))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    bool failed = false;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        if (m_oids.find(object_key[idx].key.object_id) == m_oids.end())
        {
            object_statuses[idx] = SAI_STATUS_INVALID_OBJECT_ID;

            failed = true;

            continue;
        }

        fillCounters(number_of_counters, counter_ids, &counters[(size_t)idx * number_of_counters]);

        object_statuses[idx] = SAI_STATUS_SUCCESS;
    }

    return scope.done(failed ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS, object_count, object_statuses);
}

sai_status_t MockSai::bulkClearStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::BULK_CLEAR_STATS, object_type, object_count);

    if (!m_config.bulkStatsSupported)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    if (object_count && (!object_key || !object_statuses))
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        object_statuses[idx] = SAI_STATUS_SUCCESS;
    }

    return scope.done(SAI_STATUS_SUCCESS, object_count, object_statuses);
}

uint32_t MockSai::statModes() const
{
    SWSS_LOG_ENTER();

    // syncd polls a counter in bulk only if its bulk modes are reported

    uint32_t modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;

    if (m_config.bulkStatsSupported)
    {
        modes |= SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_CLEAR | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
    }

    return modes;
}

sai_status_t MockSai::queryStatsCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, object_type, 1);

    auto info = sai_metadata_get_object_type_info(object_type);

    if (stats_capability == nullptr || info == nullptr)
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    if (info->statenum == nullptr)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    uint32_t count = (uint32_t)info->statenum->valuescount;

    if (stats_capability->count < count || stats_capability->list == nullptr)
    {
        stats_capability->count = count;

        return scope.done(SAI_STATUS_BUFFER_OVERFLOW);
    }

    for (uint32_t idx = 0; idx < count; idx++)
    {
        stats_capability->list[idx].stat_enum = (sai_stat_id_t)info->statenum->values[idx];
        stats_capability->list[idx].stat_modes = statModes();
    }

    stats_capability->count = count;

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::queryStatsStCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, object_type, 1);

    auto info = sai_metadata_get_object_type_info(object_type);

    if (stats_capability == nullptr || info == nullptr)
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    if (info->statenum == nullptr)
    {
        return scope.done(SAI_STATUS_NOT_SUPPORTED);
    }

    uint32_t count = (uint32_t)info->statenum->valuescount;

    if (stats_capability->count < count || stats_capability->list == nullptr)
    {
        stats_capability->count = count;

        return scope.done(SAI_STATUS_BUFFER_OVERFLOW);
    }

    for (uint32_t idx = 0; idx < count; idx++)
    {
        stats_capability->list[idx].capability.stat_enum = (sai_stat_id_t)info->statenum->values[idx];
        stats_capability->list[idx].capability.stat_modes = statModes();
        stats_capability->list[idx].minimal_polling_interval = 1000000;
    }

    stats_capability->count = count;

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::flushFdbEntries(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::FLUSH_FDB, SAI_OBJECT_TYPE_FDB_ENTRY, 0);

    sai_object_id_t bvId = SAI_NULL_OBJECT_ID;
    sai_object_id_t bridgePort = SAI_NULL_OBJECT_ID;

    int32_t entryType = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

    for (uint32_t idx = 0; idx < attrCount && attrList; idx++)
    {
        switch (attrList[idx].id)
        {
            case SAI_FDB_FLUSH_ATTR_BV_ID:
                bvId = attrList[idx].value.oid;
                break;

            case SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID:
                bridgePort = attrList[idx].value.oid;
                break;

            case SAI_FDB_FLUSH_ATTR_ENTRY_TYPE:
                entryType = attrList[idx].value.s32;
                break;

            default:
                break;
        }
    }

    uint64_t flushed = 0;

    for (auto it = m_entries.begin(); it != m_entries.end();)
    {
        const auto& key = it->first;

        if (key.objecttype != SAI_OBJECT_TYPE_FDB_ENTRY ||
                key.objectkey.key.fdb_entry.switch_id != switchId ||
                (bvId != SAI_NULL_OBJECT_ID && key.objectkey.key.fdb_entry.bv_id != bvId))
        {
            ++it;
            continue;
        }

        int32_t type = SAI_FDB_ENTRY_TYPE_DYNAMIC;

        sai_object_id_t port = SAI_NULL_OBJECT_ID;

        for (const auto& stored: it->second.attrs)
        {
            if (stored.attr.id == SAI_FDB_ENTRY_ATTR_TYPE)
            {
                type = stored.attr.value.s32;
            }
            else if (stored.attr.id == SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID)
            {
                port = stored.attr.value.oid;
            }
        }

        bool typeMatches = entryType == SAI_FDB_FLUSH_ENTRY_TYPE_ALL ||
            (entryType == SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC && type == SAI_FDB_ENTRY_TYPE_DYNAMIC) ||
            (entryType == SAI_FDB_FLUSH_ENTRY_TYPE_STATIC && type == SAI_FDB_ENTRY_TYPE_STATIC);

        if (!typeMatches || (bridgePort != SAI_NULL_OBJECT_ID && port != bridgePort))
        {
            ++it;
            continue;
        }

        it = m_entries.erase(it);

        m_counts[(int32_t)SAI_OBJECT_TYPE_FDB_ENTRY]--;

        flushed++;
    }

    SWSS_LOG_INFO("flushed %" PRIu64 " fdb entries", flushed);

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, objectType, 1);

    if (count == nullptr)
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    const int64_t capacity = 1000000;

    auto it = m_counts.find((int32_t)objectType);

    int64_t used = it == m_counts.end() ? 0 : it->second;

    *count = (uint64_t)std::max<int64_t>(0, capacity - used);

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, object_type, 1);

    auto md = sai_metadata_get_attr_metadata(object_type, attr_id);

    if (md == nullptr || capability == nullptr)
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    capability->create_implemented = !md->isreadonly;
    capability->set_implemented = md->iscreateandset;
    capability->get_implemented = true;

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, object_type, 1);

    auto md = sai_metadata_get_attr_metadata(object_type, attr_id);

    if (md == nullptr || enum_values_capability == nullptr || md->enummetadata == nullptr)
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    uint32_t count = (uint32_t)md->enummetadata->valuescount;

    if (enum_values_capability->count < count || enum_values_capability->list == nullptr)
    {
        enum_values_capability->count = count;

        return scope.done(SAI_STATUS_BUFFER_OVERFLOW);
    }

    for (uint32_t idx = 0; idx < count; idx++)
    {
        enum_values_capability->list[idx] = md->enummetadata->values[idx];
    }

    enum_values_capability->count = count;

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_object_type_t MockSai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::OBJECT_TYPE_QUERY, SAI_OBJECT_TYPE_NULL, 1);

    if (objectId == SAI_NULL_OBJECT_ID)
    {
        return SAI_OBJECT_TYPE_NULL;
    }

    auto objectType = (sai_object_type_t)(objectId >> MOCK_OID_TYPE_SHIFT);

    if (!sai_metadata_is_object_type_valid(objectType))
    {
        scope.done(SAI_STATUS_INVALID_OBJECT_ID);

        return SAI_OBJECT_TYPE_NULL;
    }

    return objectType;
}

sai_object_id_t MockSai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::SWITCH_ID_QUERY, SAI_OBJECT_TYPE_NULL, 1);

    if (objectId == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto it = m_switches.find(switchIndexOf(objectId));

    if (it == m_switches.end())
    {
        scope.done(SAI_STATUS_INVALID_OBJECT_ID);

        return SAI_NULL_OBJECT_ID;
    }

    return it->second.switchRid;
}

sai_status_t MockSai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, SAI_OBJECT_TYPE_NULL, 1);

    return scope.done(SAI_STATUS_SUCCESS);
}

sai_status_t MockSai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    MUTEX();
    SWSS_LOG_ENTER();

    CallScope scope(*this, Op::QUERY, SAI_OBJECT_TYPE_NULL, 1);

    if (version == nullptr)
    {
        return scope.done(SAI_STATUS_INVALID_PARAMETER);
    }

    *version = SAI_API_VERSION;

    return scope.done(SAI_STATUS_SUCCESS);
}
