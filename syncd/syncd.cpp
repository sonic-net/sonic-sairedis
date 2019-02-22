#include "syncd.h"
#include "syncd_saiswitch.h"
#include "sairedis.h"
#include "syncd_flex_counter.h"
#include "swss/tokenize.h"
#include <limits.h>

#include "swss/warm_restart.h"
#include "swss/table.h"

extern "C" {
#include <sai.h>
}

#include <iostream>
#include <map>
#include <unordered_map>

#define DEF_SAI_WARM_BOOT_DATA_FILE "/var/warmboot/sai-warmboot.bin"

/**
 * @brief Global mutex for thread synchronization
 *
 * Purpose of this mutex is to synchronize multiple threads like main thread,
 * counters and notifications as well as all operations which require multiple
 * Redis DB access.
 *
 * For example: query DB for next VID id number, and then put map RID and VID
 * to Redis. From syncd point of view this entire operation should be atomic
 * and no other thread should access DB or make assumption on previous
 * information until entire operation will finish.
 */
std::mutex g_mutex;

std::shared_ptr<swss::RedisClient>          g_redisClient;
std::shared_ptr<swss::ProducerTable>        getResponse;
std::shared_ptr<swss::NotificationProducer> notifications;

/*
 * TODO: Those are hard coded values for mlnx integration for v1.0.1 they need
 * to be updated.
 *
 * Also DEVICE_MAC_ADDRESS is not present in saiswitch.h
 */
std::map<std::string, std::string> gProfileMap;

/**
 * @brief Contains map of all created switches.
 *
 * This syncd implementation supports only one switch but it's written in
 * a way that could be extended to use multiple switches in the future, some
 * refactoring needs to be made in marked places.
 *
 * To support multiple switches VIDTORID and RIDTOVID db entries needs to be
 * made per switch like HIDDEN and LANES. Best way is to wrap vid/rid map to
 * functions that will return right key.
 *
 * Key is switch VID.
 */
std::map<sai_object_id_t, std::shared_ptr<SaiSwitch>> switches;

/**
 * @brief set of objects removed by user when we are in init view mode. Those
 * could be vlan members, bridge ports etc.
 *
 * We need this list to later on not put them back to temp view mode when doing
 * populate existing objects in apply view mode.
 *
 * Object ids here a VIDs.
 */
std::set<sai_object_id_t> initViewRemovedVidSet;

/*
 * When set to true will enable DB vs ASIC consistency check after comparison
 * logic.
 */
bool g_enableConsistencyCheck = false;

/*
 * By default we are in APPLY mode.
 */
volatile bool g_asicInitViewMode = false;

/*
 * SAI switch global needed for RPC server and for remove_switch
 */
sai_object_id_t gSwitchId;

struct cmdOptions
{
    bool diagShell;
    bool useTempView;
    int startType;
    bool disableExitSleep;
    bool enableUnittests;
    std::string profileMapFile;
#ifdef SAITHRIFT
    bool run_rpc_server;
    std::string portMapFile;
#endif // SAITHRIFT
    ~cmdOptions() {}
};

cmdOptions options;

bool enableUnittests()
{
    SWSS_LOG_ENTER();

    return options.enableUnittests;
}

bool isInitViewMode()
{
    SWSS_LOG_ENTER();

    return g_asicInitViewMode && options.useTempView;
}

bool g_veryFirstRun = false;

void exit_and_notify(int status) __attribute__ ((__noreturn__));

void exit_and_notify(
        _In_ int status)
{
    SWSS_LOG_ENTER();

    try
    {
        if (notifications != NULL)
        {
            std::vector<swss::FieldValueTuple> entry;

            SWSS_LOG_NOTICE("sending switch_shutdown_request notification to OA");

            notifications->send("switch_shutdown_request", "", entry);

            SWSS_LOG_NOTICE("notification send successfull");
        }
    }
    catch(const std::exception &e)
    {
        SWSS_LOG_ERROR("Runtime error: %s", e.what());
    }
    catch(...)
    {
        SWSS_LOG_ERROR("Unknown runtime error");
    }

    if (options.disableExitSleep)
    {
        exit(status);
    }

    SWSS_LOG_WARN("sleep forever to keep data plane active");

    while (true)
    {
        sleep(UINT_MAX);

        SWSS_LOG_NOTICE("sleep ended, sleeping again");
    }
}

void sai_diag_shell(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    /*
     * This is currently blocking API on broadcom, it will block until we exit
     * shell.
     */

    while (true)
    {
        sai_attribute_t attr;
        attr.id = SAI_SWITCH_ATTR_SWITCH_SHELL_ENABLE;
        attr.value.booldata = true;

        status = sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to enable switch shell: %s",
                    sai_serialize_status(status).c_str());
            return;
        }

        sleep(1);
    }
}

/*
 * Defined bit position on sairedis VID where object type and switch id is
 * located.
 */

#define OT_POSITION     48
#define SWID_POSITION   56

/*
 * NOTE: those redis functions could go to librediscommon etc so syncd could
 * link against it, so we don't have to duplicate code.
 */

sai_object_id_t redis_construct_object_id(
        _In_ sai_object_type_t object_type,
        _In_ int switch_index,
        _In_ uint64_t real_id)
{
    SWSS_LOG_ENTER();

    return (sai_object_id_t)(((uint64_t)switch_index << SWID_POSITION) | ((uint64_t)object_type << OT_POSITION) | real_id);
}

sai_object_type_t redis_sai_object_type_query(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (object_id == SAI_NULL_OBJECT_ID)
    {
        return SAI_OBJECT_TYPE_NULL;
    }

    sai_object_type_t ot = (sai_object_type_t)((object_id >> OT_POSITION) & 0xFF);

    if (!sai_metadata_is_object_type_valid(ot))
    {
        SWSS_LOG_THROW("invalid object id 0x%lx", object_id);
    }

    return ot;
}

int redis_get_switch_id_index(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_type_t switch_object_type = redis_sai_object_type_query(switch_id);

    if (switch_object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        return (int)((switch_id >> SWID_POSITION) & 0xFF);
    }

    SWSS_LOG_THROW("object type of switch %s is %s, should be SWITCH",
            sai_serialize_object_id(switch_id).c_str(),
            sai_serialize_object_type(switch_object_type).c_str());
}

sai_object_id_t redis_sai_switch_id_query(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return oid;
    }

    sai_object_type_t object_type = redis_sai_object_type_query(oid);

    if (object_type == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type of oid 0x%lx", oid);
    }

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        return oid;
    }

    /*
     * Each VID contains switch index at constant position.
     *
     * We extract this index from VID and we create switch ID (VID) for
     * specific object. We can do this for each object.
     */

    int sw_index = (int)((oid >> SWID_POSITION) & 0xFF);

    sai_object_id_t switch_id = redis_construct_object_id(SAI_OBJECT_TYPE_SWITCH, sw_index, sw_index);

    return switch_id;
}

sai_object_id_t redis_create_virtual_object_id(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: switch ID is VID switch ID from sairedis.
     */

    /*
     * Check if object type is in valid range.
     */

    if (!sai_metadata_is_object_type_valid(object_type))
    {
        SWSS_LOG_THROW("invalid object type: %s", sai_serialize_object_type(object_type).c_str());
    }

    /*
     * Switch id is deterministic and it comes from sairedis so make check here
     * that we will not use this for creating switch VIDs.
     */

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_THROW("this function should not be used to create VID for switch id");
    }

    uint64_t virtual_id = g_redisClient->incr(VIDCOUNTER);

    int switch_index =  redis_get_switch_id_index(switch_id);

    sai_object_id_t vid = redis_construct_object_id(object_type, switch_index, virtual_id);

    auto info = sai_metadata_get_object_type_info(object_type);

    SWSS_LOG_DEBUG("created virtual object id 0x%lx for object type %s",
            vid,
            info->objecttypename);

    return vid;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> local_rid_to_vid;
std::unordered_map<sai_object_id_t, sai_object_id_t> local_vid_to_rid;

void save_rid_and_vid_to_local(
        _In_ sai_object_id_t rid,
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    local_rid_to_vid[rid] = vid;
    local_vid_to_rid[vid] = rid;
}

void remove_rid_and_vid_from_local(
        _In_ sai_object_id_t rid,
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    local_rid_to_vid.erase(rid);
    local_vid_to_rid.erase(vid);
}

/*
 * This method will create VID for actual RID retrieved from device when doing
 * GET api and snooping while in init view mode.
 *
 * This function should not be used to create VID for SWITCH object type.
 */
sai_object_id_t translate_rid_to_vid(
        _In_ sai_object_id_t rid,
        _In_ sai_object_id_t switch_vid)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: switch_vid here is Virtual ID of switch for which we need
     * create VID for given RID.
     */

    if (rid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_DEBUG("translated RID null to VID null");

        return SAI_NULL_OBJECT_ID;
    }

    auto it = local_rid_to_vid.find(rid);

    if (it != local_rid_to_vid.end())
    {
        return it->second;
    }

    sai_object_id_t vid;

    std::string str_rid = sai_serialize_object_id(rid);

    auto pvid = g_redisClient->hget(RIDTOVID, str_rid);

    if (pvid != NULL)
    {
        /*
         * Object exists.
         */

        std::string str_vid = *pvid;

        sai_deserialize_object_id(str_vid, vid);

        SWSS_LOG_DEBUG("translated RID 0x%lx to VID 0x%lx", rid, vid);

        return vid;
    }

    SWSS_LOG_DEBUG("spotted new RID 0x%lx", rid);

    sai_object_type_t object_type = sai_object_type_query(rid);

    if (object_type == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("sai_object_type_query returned NULL type for RID 0x%lx", rid);
    }

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        /*
         * Switch ID should be already inside local db or redis db when we
         * created switch, so we should never get here.
         */

        SWSS_LOG_THROW("RID 0x%lx is switch object, but not in local or redis db, bug!", rid);
    }

    vid = redis_create_virtual_object_id(switch_vid, object_type);

    SWSS_LOG_DEBUG("translated RID 0x%lx to VID 0x%lx", rid, vid);

    std::string str_vid = sai_serialize_object_id(vid);

    /*
     * TODO: This must be ATOMIC.
     *
     * TODO: To support multiple switches we need this map per switch;
     */

    g_redisClient->hset(RIDTOVID, str_rid, str_vid);
    g_redisClient->hset(VIDTORID, str_vid, str_rid);

    save_rid_and_vid_to_local(rid, vid);

    return vid;
}

/**
 * @brief Check if RID exists on the ASIC DB.
 *
 * @param rid Real object id to check.
 *
 * @return True if exists or SAI_NULL_OBJECT_ID, otherwise false.
 */
bool check_rid_exists(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    if (rid == SAI_NULL_OBJECT_ID)
        return true;

    if (local_rid_to_vid.find(rid) != local_rid_to_vid.end())
        return true;

    std::string str_rid = sai_serialize_object_id(rid);

    auto pvid = g_redisClient->hget(RIDTOVID, str_rid);

    if (pvid != NULL)
        return true;

    return false;
}

void translate_list_rid_to_vid(
        _In_ sai_object_list_t &element,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_rid_to_vid(element.list[i], switch_id);
    }
}

/*
 * This method is required to translate RID to VIDs when we are doing snoop for
 * new ID's in init view mode, on in apply view mode when we are executing GET
 * api, and new object RIDs were spotted the we will create new VIDs for those
 * objects and we will put them to redis db.
 */
void translate_rid_to_vid_list(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * We receive real id's here, if they are new then create new VIDs for them
     * and put in db, if entry exists in db, use it.
     *
     * NOTE: switch_id is VID of switch on which those RIDs are provided.
     */

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %x, attribute %d", object_type, attr.id);
        }

        /*
         * TODO: Many times we do switch for list of attributes to perform some
         * operation on each oid from that attribute, we should provide clever
         * way via sai metadata utils to get that.
         */

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = translate_rid_to_vid(attr.value.oid, switch_id);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                translate_list_rid_to_vid(attr.value.objlist, switch_id);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    attr.value.aclfield.data.oid = translate_rid_to_vid(attr.value.aclfield.data.oid, switch_id);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    translate_list_rid_to_vid(attr.value.aclfield.data.objlist, switch_id);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    attr.value.aclaction.parameter.oid = translate_rid_to_vid(attr.value.aclaction.parameter.oid, switch_id);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    translate_list_rid_to_vid(attr.value.aclaction.parameter.objlist, switch_id);
                break;

            default:

                /*
                 * If in future new attribute with object id will be added this
                 * will make sure that we will need to add handler here.
                 */

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is object id, but not processed, FIXME", meta->attridname);
                }

                break;
        }
    }
}

/*
 * NOTE: We could have in metadata utils option to execute function on each
 * object on oid like this.  Problem is that we can't then add extra
 * parameters.
 */

sai_object_id_t translate_vid_to_rid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_DEBUG("translated VID null to RID null");

        return SAI_NULL_OBJECT_ID;
    }

    auto it = local_vid_to_rid.find(vid);

    if (it != local_vid_to_rid.end())
    {
        return it->second;
    }

    std::string str_vid = sai_serialize_object_id(vid);

    std::string str_rid;

    auto prid = g_redisClient->hget(VIDTORID, str_vid);

    if (prid == NULL)
    {
        if (isInitViewMode())
        {
            /*
             * If user created object that is object id, then it should not
             * query attributes of this object in init view mode, because he
             * knows all attributes passed to that object.
             *
             * NOTE: This may be a problem for some objects in init view mode.
             * We will need to revisit this after checking with real SAI
             * implementation.  Problem here may be that user will create some
             * object and actually will need to to query some of it's values,
             * like buffer limitations etc, mostly probably this will happen on
             * SWITCH object.
             */

            SWSS_LOG_THROW("can't get RID in init view mode - don't query created objects");
        }

        SWSS_LOG_THROW("unable to get RID for VID: 0x%lx", vid);
    }

    str_rid = *prid;

    sai_object_id_t rid;

    sai_deserialize_object_id(str_rid, rid);

    /*
     * We got this RID from redis db, so put it also to local db so it will be
     * faster to retrieve it late on.
     */

    local_vid_to_rid[vid] = rid;

    SWSS_LOG_DEBUG("translated VID 0x%lx to RID 0x%lx", vid, rid);

    return rid;
}

void translate_list_vid_to_rid(
        _In_ sai_object_list_t &element)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_vid_to_rid(element.list[i]);
    }
}

void translate_vid_to_rid_list(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * All id's received from sairedis should be virtual, so lets translate
     * them to real id's before we execute actual api.
     */

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %x, attribute %d", object_type, attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = translate_vid_to_rid(attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                translate_list_vid_to_rid(attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    attr.value.aclfield.data.oid = translate_vid_to_rid(attr.value.aclfield.data.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    translate_list_vid_to_rid(attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    attr.value.aclaction.parameter.oid = translate_vid_to_rid(attr.value.aclaction.parameter.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    translate_list_vid_to_rid(attr.value.aclaction.parameter.objlist);
                break;

            default:

                /*
                 * If in future new attribute with object id will be added this
                 * will make sure that we will need to add handler here.
                 */

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is object id, but not processed, FIXME", meta->attridname);
                }

                break;
        }
    }
}

void snoop_get_attr(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ const std::string &attr_id,
        _In_ const std::string &attr_value)
{
    SWSS_LOG_ENTER();

    /*
     * Note: str_object_type + ":" + str_object_id is meta_key we can us that
     * here later on.
     */

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string prefix = "";

    if (isInitViewMode())
    {
        prefix = TEMP_PREFIX;
    }

    std::string key = prefix + (ASIC_STATE_TABLE + (":" + str_object_type + ":" + str_object_id));

    SWSS_LOG_DEBUG("%s", key.c_str());


    g_redisClient->hset(key, attr_id, attr_value);
}

void snoop_get_oid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        /*
         * If snooped oid is NULL then we don't need take any action.
         */

        return;
    }

    /*
     * We need use redis version of object type query here since we are
     * operating on VID value, and syncd is compiled against real SAI
     * implementation which has different function sai_object_type_query.
     */

    sai_object_type_t object_type = redis_sai_object_type_query(vid);

    std::string str_vid = sai_serialize_object_id(vid);

    snoop_get_attr(object_type, str_vid, "NULL", "NULL");
}

void snoop_get_oid_list(
        _In_ const sai_object_list_t &list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < list.count; i++)
    {
        snoop_get_oid(list.list[i]);
    }
}

void snoop_get_attr_value(
        _In_ const std::string &str_object_id,
        _In_ const sai_attr_metadata_t *meta,
        _In_ const sai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    std::string value = sai_serialize_attr_value(*meta, attr);

    SWSS_LOG_DEBUG("%s:%s", meta->attridname, value.c_str());

    snoop_get_attr(meta->objecttype, str_object_id, meta->attridname, value);
}

void snoop_get_response(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: this method is operating on VIDs, all RIDs were translated outside
     * this method.
     */

    /*
     * Vlan (including vlan 1) will need to be put into TEMP view this should
     * also be valid for all objects that were queried.
     */

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        const sai_attribute_t &attr = attr_list[idx];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %d, attribute %d", object_type, attr.id);
        }

        /*
         * We should snoop oid values even if they are readonly we just note in
         * temp view that those objects exist on switch.
         */

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                snoop_get_oid(attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                snoop_get_oid_list(attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    snoop_get_oid(attr.value.aclfield.data.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    snoop_get_oid_list(attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    snoop_get_oid(attr.value.aclaction.parameter.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    snoop_get_oid_list(attr.value.aclaction.parameter.objlist);
                break;

            default:

                /*
                 * If in future new attribute with object id will be added this
                 * will make sure that we will need to add handler here.
                 */

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is object id, but not processed, FIXME", meta->attridname);
                }

                break;
        }

        if (SAI_HAS_FLAG_READ_ONLY(meta->flags))
        {
            /*
             * If value is read only, we skip it, since after syncd restart we
             * won't be able to set/create it anyway.
             */

            continue;
        }

        if (meta->objecttype == SAI_OBJECT_TYPE_PORT &&
                meta->attrid == SAI_PORT_ATTR_HW_LANE_LIST)
        {
            /*
             * Skip port lanes for now since we don't create ports.
             */

            SWSS_LOG_INFO("skipping %s for %s", meta->attridname, str_object_id.c_str());
            continue;
        }

        /*
         * Put non readonly, and non oid attribute value to temp view.
         *
         * NOTE: This will also put create-only attributes to view, and after
         * syncd hard reinit we will not be able to do "SET" on that attribute.
         *
         * Similar action can happen when we will do this on asicSet during
         * apply view.
         */

        snoop_get_attr_value(str_object_id, meta, attr);
    }
}

void internal_syncd_get_send(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ sai_status_t status,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        translate_rid_to_vid_list(object_type, switch_id, attr_count, attr_list);

        /*
         * Normal serialization + translate RID to VID.
         */

        entry = SaiAttributeList::serialize_attr_list(
                object_type,
                attr_count,
                attr_list,
                false);

        /*
         * All oid values here are VIDs.
         */

        snoop_get_response(object_type, str_object_id, attr_count, attr_list);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        /*
         * In this case we got correct values for list, but list was too small
         * so serialize only count without list itself, sairedis will need to
         * take this into account when deserialize.
         *
         * If there was a list somewhere, count will be changed to actual value
         * different attributes can have different lists, many of them may
         * serialize only count, and will need to support that on the receiver.
         */

        entry = SaiAttributeList::serialize_attr_list(
                object_type,
                attr_count,
                attr_list,
                true);
    }
    else
    {
        /*
         * Some other error, don't send attributes at all.
         */
    }

    for (const auto &e: entry)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(e).c_str(), fvValue(e).c_str());
    }

    std::string str_status = sai_serialize_status(status);

    SWSS_LOG_INFO("sending response for GET api with status: %s", str_status.c_str());

    /*
     * Since we have only one get at a time, we don't have to serialize object
     * type and object id, only get status is required to be returned.  Get
     * response will not put any data to table, only queue is used.
     */

    getResponse->set(str_status, entry, "getresponse");

    SWSS_LOG_INFO("response for GET api was send");
}

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = gProfileMap.find(variable);

    if (it == gProfileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

std::map<std::string, std::string>::iterator gProfileIter = gProfileMap.begin();

int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        gProfileIter = gProfileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (gProfileIter == gProfileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = gProfileIter->first.c_str();
    *value = gProfileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    gProfileIter++;

    return 0;
}

sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

void startDiagShell()
{
    SWSS_LOG_ENTER();

    if (options.diagShell)
    {
        SWSS_LOG_NOTICE("starting diag shell thread");

        /*
         * TODO actual switch id must be supplied
         */

        std::thread diag_shell_thread = std::thread(sai_diag_shell, SAI_NULL_OBJECT_ID);

        diag_shell_thread.detach();
    }
}

void on_switch_create(
        _In_ sai_object_id_t switch_vid)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_rid = translate_vid_to_rid(switch_vid);

    if (switches.size() > 0)
    {
        SWSS_LOG_THROW("creating multiple switches is not supported yet, FIXME");
    }

    /*
     * All needed data to populate switch should be obtained inside SaiSwitch
     * constructor, like getting all queues, ports, etc.
     */

    switches[switch_vid] = std::make_shared<SaiSwitch>(switch_vid, switch_rid);

    startDiagShell();
}

void on_switch_remove(
        _In_ sai_object_id_t switch_id_vid)
    __attribute__ ((__noreturn__));

void on_switch_remove(
        _In_ sai_object_id_t switch_id_vid)
{
    SWSS_LOG_ENTER();

    /*
     * On remove switch there should be extra action all local objects and
     * redis object should be removed on remove switch local and redis db
     * objects should be cleared.
     *
     * Currently we don't want to remove switch so we don't need this method,
     * but lets put this as a safety check.
     *
     * To support multiple switches this function needs to be refactored.
     */

    SWSS_LOG_THROW("remove switch is not implemented, FIXME");
}

/**
 * @brief Determines whether attribute is "workaround" attribute for SET API.
 *
 * Some attributes are not supported on SET API on different platforms.
 * For example SAI_SWITCH_ATTR_SRC_MAC_ADDRESS.
 *
 * @param[in] objecttype Object type.
 * @param[in] attrid Attribute Id.
 * @param[in] status Status from SET API.
 *
 * @return True if error from SET API can be ignored, false otherwise.
 */
bool is_set_attribute_workaround(
        _In_ sai_object_type_t objecttype,
        _In_ sai_attr_id_t attrid,
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    if (status == SAI_STATUS_SUCCESS)
    {
        return false;
    }

    if (objecttype == SAI_OBJECT_TYPE_SWITCH &&
            attrid == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
    {
        SWSS_LOG_WARN("setting %s failed: %s, not all platforms support this attribute",
                sai_metadata_get_attr_metadata(objecttype, attrid)->attridname,
                sai_serialize_status(status).c_str());

        return true;
    }

    return false;
}

sai_status_t handle_generic(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * TODO: Could deserialize to meta key and then we could combine with non
     * object id.
     */

    sai_object_id_t object_id;
    sai_deserialize_object_id(str_object_id, object_id);

    SWSS_LOG_DEBUG("calling %s for %s",
            sai_serialize_common_api(api).c_str(),
            sai_serialize_object_type(object_type).c_str());

    sai_object_meta_key_t meta_key;

    meta_key.objecttype = object_type;
    meta_key.objectkey.key.object_id = object_id;

    /*
     * We need to do translate vid/rid except for create, since create will
     * create new RID value, and we will have to map them to VID we received in
     * create query.
     */

    /*
     * TODO: use metadata utils.
     */

    auto info = sai_metadata_get_object_type_info(object_type);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
    }

    switch (api)
    {
        case SAI_COMMON_API_CREATE:

            {
                /*
                 * Object id is VID, we can use it to extract switch id.
                 */

                sai_object_id_t switch_id = redis_sai_switch_id_query(object_id);

                if (switch_id == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_THROW("invalid switch_id translated from VID 0x%lx", object_id);
                }

                if (object_type != SAI_OBJECT_TYPE_SWITCH)
                {
                    /*
                     * When we creating switch, then switch_id parameter is
                     * ignored, but we can't convert it using vid to rid map,
                     * since rid doesn't exist yet, so skip translate for switch,
                     * but use translate for all other objects.
                     */

                    switch_id = translate_vid_to_rid(switch_id);
                }
                else
                {
                    if (switches.size() > 0)
                    {
                        /*
                         * NOTE: to support multiple switches we need support
                         * here for create.
                         */

                        SWSS_LOG_THROW("creating multiple switches is not supported yet, FIXME");
                    }
                }

                sai_status_t status = info->create(&meta_key, switch_id, attr_count, attr_list);

                if (status == SAI_STATUS_SUCCESS)
                {
                    sai_object_id_t real_object_id = meta_key.objectkey.key.object_id;

                    /*
                     * Object was created so new object id was generated we
                     * need to save virtual id's to redis db.
                     */

                    std::string str_vid = sai_serialize_object_id(object_id);
                    std::string str_rid = sai_serialize_object_id(real_object_id);

                    /*
                     * TODO: This must be ATOMIC.
                     *
                     * To support multiple switches vid/rid map must be per switch.
                     */

                    {

                        g_redisClient->hset(VIDTORID, str_vid, str_rid);
                        g_redisClient->hset(RIDTOVID, str_rid, str_vid);

                        save_rid_and_vid_to_local(real_object_id, object_id);
                    }

                    SWSS_LOG_INFO("saved VID %s to RID %s", str_vid.c_str(), str_rid.c_str());

                    if (object_type == SAI_OBJECT_TYPE_SWITCH)
                    {
                        on_switch_create(switch_id);
                        gSwitchId = real_object_id;
                        SWSS_LOG_NOTICE("Initialize gSwitchId with ID = 0x%lx", gSwitchId);
                    }
                }

                return status;
            }

        case SAI_COMMON_API_REMOVE:

            {
                sai_object_id_t rid = translate_vid_to_rid(object_id);

                meta_key.objectkey.key.object_id = rid;

                sai_status_t status = info->remove(&meta_key);

                if (status == SAI_STATUS_SUCCESS)
                {
                    std::string str_vid = sai_serialize_object_id(object_id);
                    std::string str_rid = sai_serialize_object_id(rid);

                    /*
                     * TODO: This must be ATOMIC.
                     */

                    {

                        g_redisClient->hdel(VIDTORID, str_vid);
                        g_redisClient->hdel(RIDTOVID, str_rid);

                        remove_rid_and_vid_from_local(rid, object_id);
                    }

                    if (object_type == SAI_OBJECT_TYPE_SWITCH)
                    {
                        on_switch_remove(object_id);
                    }
                    else
                    {
                        /*
                         * Removing some object succeeded. Let's check if that
                         * object was default created object, eg. vlan member.
                         * Then we need to update default created object map in
                         * SaiSwitch to be in sync, and be prepared for apply
                         * view to transfer those synced default created
                         * objects to temporary view when it will be created,
                         * since that will be out basic switch state.
                         *
                         * TODO: there can be some issues with reference count
                         * like for schedulers on scheduler groups since they
                         * should have internal references, and we still need
                         * to create dependency tree from saiDiscovery and
                         * update those references to track them, this is
                         * printed in metadata sanitycheck as "default value
                         * needs to be stored".
                         *
                         * TODO lets add SAI metadata flag for that this will
                         * also needs to be of internal/vendor default but we
                         * can already deduce that.
                         */

                        sai_object_id_t switch_vid = redis_sai_switch_id_query(object_id);

                        if (switches.at(switch_vid)->isDiscoveredRid(rid))
                        {
                            switches.at(switch_vid)->removeExistingObjectReference(rid);
                        }
                    }
                }

                return status;
            }

        case SAI_COMMON_API_SET:

            {
                sai_object_id_t rid = translate_vid_to_rid(object_id);

                meta_key.objectkey.key.object_id = rid;

                sai_status_t status = info->set(&meta_key, attr_list);

                if (is_set_attribute_workaround(meta_key.objecttype, attr_list->id, status))
                {
                    return SAI_STATUS_SUCCESS;
                }

                return status;
            }

        case SAI_COMMON_API_GET:

            {
                sai_object_id_t rid = translate_vid_to_rid(object_id);

                meta_key.objectkey.key.object_id = rid;

                return info->get(&meta_key, attr_count, attr_list);
            }

        default:

            SWSS_LOG_THROW("common api (%s) is not implemented", sai_serialize_common_api(api).c_str());
    }
}

void translate_vid_to_rid_non_object_id(
        _Inout_ sai_object_meta_key_t &meta_key)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    if (info->isobjectid)
    {
        meta_key.objectkey.key.object_id =
            translate_vid_to_rid(meta_key.objectkey.key.object_id);

        return;
    }

    for (size_t j = 0; j < info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = info->structmembers[j];

        if (m->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            sai_object_id_t vid = m->getoid(&meta_key);

            sai_object_id_t rid = translate_vid_to_rid(vid);

            m->setoid(&meta_key, rid);
        }
    }
}

sai_status_t handle_non_object_id(
        _In_ sai_object_meta_key_t &meta_key,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    translate_vid_to_rid_non_object_id(meta_key);

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return info->create(&meta_key, SAI_NULL_OBJECT_ID, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return info->remove(&meta_key);

        case SAI_COMMON_API_SET:
            return info->set(&meta_key, attr_list);

        case SAI_COMMON_API_GET:
            return info->get(&meta_key, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("other apis not implemented");
    }
}

void sendNotifyResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    std::string str_status = sai_serialize_status(status);

    std::vector<swss::FieldValueTuple> entry;

    SWSS_LOG_NOTICE("sending response: %s", str_status.c_str());

    getResponse->set(str_status, entry, "notify");
}

void clearTempView()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing current TEMP VIEW");

    SWSS_LOG_TIMER("clear temp view");

    std::string pattern = TEMP_PREFIX + (ASIC_STATE_TABLE + std::string(":*"));

    /*
     * TODO this must be ATOMIC, and could use lua script.
     *
     * We need to expose api to execute user lua script not only predefined.
     */


    for (const auto &key: g_redisClient->keys(pattern))
    {
        g_redisClient->del(key);
    }

    /*
     * Also clear list of objects removed in init view mode.
     */

    initViewRemovedVidSet.clear();
}

void InspectAsic()
{
    SWSS_LOG_ENTER();

    // Fetch all the keys from ASIC DB
    // Loop through all the keys in ASIC DB
    std::string pattern = ASIC_STATE_TABLE + std::string(":*");
    for (const auto &key: g_redisClient->keys(pattern))
    {
        // ASIC_STATE:objecttype:objectid (object id may contain ':')
        auto start = key.find_first_of(":");
        if (start == std::string::npos)
        {
            SWSS_LOG_ERROR("invalid ASIC_STATE_TABLE %s: no start :", key.c_str());
            break;
        }
        auto mid = key.find_first_of(":", start + 1);
        if (mid == std::string::npos)
        {
            SWSS_LOG_ERROR("invalid ASIC_STATE_TABLE %s: no mid :", key.c_str());
            break;
        }
        auto str_object_type = key.substr(start + 1, mid - start - 1);
        auto str_object_id  = key.substr(mid + 1);

        sai_object_type_t object_type;
        sai_deserialize_object_type(str_object_type, object_type);

        // Find all the attrid from ASIC DB, and use them to query ASIC
        auto hash = g_redisClient->hgetall(key);
        std::vector<swss::FieldValueTuple> values;
        for (auto &kv: hash)
        {
            const std::string &skey = kv.first;
            const std::string &svalue = kv.second;

            swss::FieldValueTuple fvt(skey, svalue);

            values.push_back(fvt);
        }

        SaiAttributeList list(object_type, values, false);

        sai_attribute_t *attr_list = list.get_attr_list();

        uint32_t attr_count = list.get_attr_count();

        SWSS_LOG_DEBUG("attr count: %u", list.get_attr_count());

        if (attr_count == 0)
        {
            // TODO: how to check ASIC on ASIC DB key with NULL:NULL hash
            continue; // Just ignore
        }

        auto info = sai_metadata_get_object_type_info(object_type);

        // Call SAI Get API on this key
        sai_status_t status;
        switch (object_type)
        {
            case SAI_OBJECT_TYPE_FDB_ENTRY:
            {
                sai_fdb_entry_t fdb_entry;
                sai_deserialize_fdb_entry(str_object_id, fdb_entry);

                fdb_entry.switch_id = translate_vid_to_rid(fdb_entry.switch_id);
                fdb_entry.bv_id = translate_vid_to_rid(fdb_entry.bv_id);

                status = sai_metadata_sai_fdb_api->get_fdb_entry_attribute(&fdb_entry, attr_count, attr_list);
                break;
            }

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            {
                sai_neighbor_entry_t neighbor_entry;
                sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);

                neighbor_entry.switch_id = translate_vid_to_rid(neighbor_entry.switch_id);
                neighbor_entry.rif_id = translate_vid_to_rid(neighbor_entry.rif_id);

                status = sai_metadata_sai_neighbor_api->get_neighbor_entry_attribute(&neighbor_entry, attr_count, attr_list);
                break;
            }

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            {
                sai_route_entry_t route_entry;
                sai_deserialize_route_entry(str_object_id, route_entry);

                route_entry.switch_id = translate_vid_to_rid(route_entry.switch_id);
                route_entry.vr_id = translate_vid_to_rid(route_entry.vr_id);

                status = sai_metadata_sai_route_api->get_route_entry_attribute(&route_entry, attr_count, attr_list);
                break;
            }

            default:
            {
                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("object %s:%s is non object id, but not handled, FIXME",
                            sai_serialize_object_type(object_type).c_str(),
                            str_object_id.c_str());
                }

                sai_object_id_t object_id;
                sai_deserialize_object_id(str_object_id, object_id);

                sai_object_meta_key_t meta_key;

                meta_key.objecttype = object_type;
                meta_key.objectkey.key.object_id = translate_vid_to_rid(object_id);

                status = info->get(&meta_key, attr_count, attr_list);
                break;
            }
        }

        if (status == SAI_STATUS_NOT_IMPLEMENTED)
        {
            SWSS_LOG_ERROR("not implemented get api: %s", str_object_type.c_str());
        }
        else if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to execute get api: %s", sai_serialize_status(status).c_str());
        }

        // Compare fields and values from ASIC_DB and SAI response
        // Log the difference
        for (uint32_t index = 0; index < attr_count; ++index)
        {
            const sai_attribute_t *attr = &attr_list[index];

            auto meta = sai_metadata_get_attr_metadata(object_type, attr->id);

            if (meta == NULL)
            {
                SWSS_LOG_ERROR("FATAL: failed to find metadata for object type %d and attr id %d", object_type, attr->id);
                break;
            }

            std::string str_attr_id = sai_serialize_attr_id(*meta);

            std::string str_attr_value = sai_serialize_attr_value(*meta, *attr, false);

            std::string hash_attr_value = hash[str_attr_id];
            if (hash_attr_value == str_attr_value)
            {
                SWSS_LOG_INFO("Matched %s redis attr %s with asic attr %s for %s:%s", str_attr_id.c_str(), hash_attr_value.c_str(), str_attr_value.c_str(), str_object_type.c_str(), str_object_id.c_str());
            }
            else
            {
                SWSS_LOG_ERROR("Failed to match %s redis attr %s with asic attr %s for %s:%s", str_attr_id.c_str(), hash_attr_value.c_str(), str_attr_value.c_str(), str_object_type.c_str(), str_object_id.c_str());
            }
        }
    }
}

sai_status_t onApplyViewInFastFastBoot()
{
    SWSS_LOG_ENTER();

    sai_switch_api_t* sai_switch_api = nullptr;
    sai_api_query(SAI_API_SWITCH, (void**)&sai_switch_api);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_FAST_API_ENABLE;
    attr.value.booldata = false;

    sai_status_t status = sai_switch_api->set_switch_attribute(gSwitchId, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to set SAI_SWITCH_ATTR_FAST_API_ENABLE=false: %s", sai_serialize_status(status).c_str());
    }

    return status;
}

sai_status_t notifySyncd(
        _In_ const std::string& op)
{
    SWSS_LOG_ENTER();

    if (!options.useTempView)
    {
        SWSS_LOG_NOTICE("received %s, ignored since TEMP VIEW is not used, returning success", op.c_str());

        sendNotifyResponse(SAI_STATUS_SUCCESS);

        return SAI_STATUS_SUCCESS;
    }

    static bool firstInitWasPerformed = false;

    if (g_veryFirstRun && firstInitWasPerformed && op == SYNCD_INIT_VIEW)
    {
        /*
         * Make sure that when second INIT view arrives, then we will jump
         * to next section, since second init view may create switch that
         * already exists and will fail with creating multiple switches
         * error.
         */

        g_veryFirstRun = false;
    }
    else if (g_veryFirstRun)
    {
        SWSS_LOG_NOTICE("very first run is TRUE, op = %s", op.c_str());

        sai_status_t status = SAI_STATUS_SUCCESS;

        /*
         * On the very first start of syncd, "compile" view is directly applied
         * on device, since it will make it easier to switch to new asic state
         * later on when we restart orch agent.
         */

        if (op == SYNCD_INIT_VIEW)
        {
            /*
             * On first start we just do "apply" directly on asic so we set
             * init to false instead of true.
             */

            g_asicInitViewMode = false;

            firstInitWasPerformed = true;

            /*
             * We need to clear current temp view to make space for new one.
             */

            clearTempView();
        }
        else if (op == SYNCD_APPLY_VIEW)
        {
            g_veryFirstRun = false;

            g_asicInitViewMode = false;

            if (options.startType == SAI_FASTFAST_BOOT)
            {
                /* fastfast boot configuration end */
                status = onApplyViewInFastFastBoot();
            }

            SWSS_LOG_NOTICE("setting very first run to FALSE, op = %s", op.c_str());
        }
        else
        {
            SWSS_LOG_THROW("unknown operation: %s", op.c_str());
        }

        sendNotifyResponse(status);

        return status;
    }

    if (op == SYNCD_INIT_VIEW)
    {
        if (g_asicInitViewMode)
        {
            SWSS_LOG_WARN("syncd is already in asic INIT VIEW mode, but received init again, orchagent restarted before apply?");
        }

        g_asicInitViewMode = true;

        clearTempView();

        /*
         * TODO: Currently as WARN to be easier to spot, later should be NOTICE.
         */

        SWSS_LOG_WARN("syncd switched to INIT VIEW mode, all op will be saved to TEMP view");

        sendNotifyResponse(SAI_STATUS_SUCCESS);
    }
    else if (op == SYNCD_APPLY_VIEW)
    {
        g_asicInitViewMode = false;

        /*
         * TODO: Currently as WARN to be easier to spot, later should be NOTICE.
         */

        SWSS_LOG_WARN("syncd received APPLY VIEW, will translate");

        sai_status_t status = syncdApplyView();

        sendNotifyResponse(status);

        if (status == SAI_STATUS_SUCCESS)
        {
            /*
             * We successfully applied new view, VID mapping could change, so we
             * need to clear local db, and all new VIDs will be queried using
             * redis.
             */

            local_rid_to_vid.clear();
            local_vid_to_rid.clear();
        }
        else
        {
            /*
             * Apply view failed. It can fail in 2 ways, ether nothing was
             * executed, on asic, or asic is inconsistent state then we should
             * die or hang
             */

            return status;
        }
    }
    else if (op == SYNCD_INSPECT_ASIC)
    {
        SWSS_LOG_NOTICE("syncd switched to INSPECT ASIC mode");

        InspectAsic();

        sendNotifyResponse(SAI_STATUS_SUCCESS);
    }
    else
    {
        SWSS_LOG_ERROR("unknown operation: %s", op.c_str());

        sendNotifyResponse(SAI_STATUS_NOT_IMPLEMENTED);

        SWSS_LOG_THROW("notify syncd %s operation failed", op.c_str());
    }

    return SAI_STATUS_SUCCESS;
}

template <typename T, typename G>
std::vector<T> extractCounterIdsGeneric(
        _In_ const swss::KeyOpFieldsValuesTuple &kco,
        _In_ G deserializeIdFn)
{
    SWSS_LOG_ENTER();

    const auto& values = kfvFieldsValues(kco);
    std::vector<T> counterIdList;
    for (const auto & v : values)
    {
        std::string field = fvField(v);
        T counterId;
        deserializeIdFn(field.c_str(), &counterId);

        counterIdList.push_back(counterId);
    }

    return std::move(counterIdList);
}

template <typename T, typename F, typename G>
sai_status_t getStatsGeneric(
        _In_ sai_object_id_t object_id,
        _In_ const swss::KeyOpFieldsValuesTuple &kco,
        _Out_ std::vector<uint64_t>& counters,
        _In_ F getStatsFn,
        _In_ G deserializeIdFn)
{
    SWSS_LOG_ENTER();

    const std::vector<T> counter_ids = extractCounterIdsGeneric<T, G>(
            kco,
            deserializeIdFn);
    counters.resize(counter_ids.size());

    return getStatsFn(
            object_id,
            (uint32_t)counter_ids.size(),
            (const sai_stat_id_t*)counter_ids.data(),
            counters.data());
}

sai_status_t processGetStatsEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);
    const std::string &str_object_type = key.substr(0, key.find(":"));
    const std::string &str_object_id = key.substr(key.find(":") + 1);

    sai_object_id_t object_id;
    sai_deserialize_object_id(str_object_id, object_id);
    sai_object_id_t rid = translate_vid_to_rid(object_id);
    sai_object_type_t object_type;
    sai_deserialize_object_type(str_object_type, object_type);

    std::vector<uint64_t> result;

    sai_status_t status = SAI_STATUS_SUCCESS;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_PORT:
            status = getStatsGeneric<sai_port_stat_t>(
                    rid,
                    kco,
                    result,
                    sai_metadata_sai_port_api->get_port_stats,
                    sai_deserialize_port_stat);
            break;
        case SAI_OBJECT_TYPE_QUEUE:
            status = getStatsGeneric<sai_queue_stat_t>(
                    rid,
                    kco,
                    result,
                    sai_metadata_sai_queue_api->get_queue_stats,
                    sai_deserialize_queue_stat);
            break;
        case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
            status = getStatsGeneric<sai_ingress_priority_group_stat_t>(
                    rid,
                    kco,
                    result,
                    sai_metadata_sai_buffer_api->get_ingress_priority_group_stats,
                    sai_deserialize_ingress_priority_group_stat);
            break;
        default:
            SWSS_LOG_ERROR("SAI object type %s not supported", str_object_type.c_str());
            status = SAI_STATUS_NOT_SUPPORTED;
    }

    std::vector<swss::FieldValueTuple> entry;

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get stats");
    }
    else
    {
        const auto& values = kfvFieldsValues(kco);
        for (size_t i = 0; i < values.size(); i++)
        {
            entry.emplace_back(fvField(values[i]), std::to_string(result[i]));
        }
    }

    getResponse->set(sai_serialize_status(status), entry, "getresponse");

    return status;
}

void on_switch_create_in_init_view(
        _In_ sai_object_id_t switch_vid,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * This needs to be refactored if we need multiple switch support.
     */

    /*
     * We can have multiple switches here, but each switch is identified by
     * SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO. This attribute is treated as key,
     * so each switch will have different hardware info.
     *
     * Currently we assume that we have only one switch.
     *
     * We can have 2 scenarios here:
     *
     * - we have multiple switches already existing, and in init view mode user
     *   will create the same switches, then since switch id are deterministic
     *   we can match them by hardware info and by switch id, it may happen
     *   that switch id will be different if user will create switches in
     *   different order, this case will be not supported unless special logic
     *   will be written to handle that case.
     *
     * - if user created switches but non of switch has the same hardware info
     *   then it means we need to create actual switch here, since user will
     *   want to query switch ports etc values, that's why on create switch is
     *   special case, and that's why we need to keep track of all switches
     *
     * Since we are creating switch here, we are sure that this switch don't
     * have any oid attributes set, so we can pass all attributes
     */

    /*
     * Multiple switches scenario with changed order:
     *
     * If orchagent will create the same switch with the same hardware info but
     * with different order since switch id is deterministic, then VID of both
     * switches will not match:
     *
     * First we can have INFO = "A" swid 0x00170000, INFO = "B" swid 0x01170001
     *
     * Then we can have INFO = "B" swid 0x00170000, INFO = "A" swid 0x01170001
     *
     * Currently we don't have good solution for that so we will throw in that case.
     */

    if (switches.size() == 0)
    {
        /*
         * There are no switches currently, so we need to create this switch so
         * user in init mode could query switch properties using GET api.
         *
         * We assume that none of attributes is object id attribute.
         *
         * This scenario can happen when you start syncd on empty database and
         * then you quit and restart it again.
         */

        sai_object_id_t switch_rid;

        sai_status_t status;

        {
            SWSS_LOG_TIMER("cold boot: create switch");
            status = sai_metadata_sai_switch_api->create_switch(&switch_rid, attr_count, attr_list);
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to create switch in init view mode: %s",
                    sai_serialize_status(status).c_str());
        }

#ifdef SAITHRIFT
        gSwitchId = switch_rid;
        SWSS_LOG_NOTICE("Initialize gSwitchId with ID = 0x%lx", gSwitchId);
#endif

        /*
         * Object was created so new object id was generated we
         * need to save virtual id's to redis db.
         */

        std::string str_vid = sai_serialize_object_id(switch_vid);
        std::string str_rid = sai_serialize_object_id(switch_rid);

        SWSS_LOG_NOTICE("created real switch VID %s to RID %s in init view mode", str_vid.c_str(), str_rid.c_str());

        /*
         * TODO: This must be ATOMIC.
         *
         * To support multiple switches vid/rid map must be per switch.
         */

        g_redisClient->hset(VIDTORID, str_vid, str_rid);
        g_redisClient->hset(RIDTOVID, str_rid, str_vid);

        save_rid_and_vid_to_local(switch_rid, switch_vid);

        /*
         * Make switch initialization and get all default data.
         */

        switches[switch_vid] = std::make_shared<SaiSwitch>(switch_vid, switch_rid);
    }
    else if (switches.size() == 1)
    {
        /*
         * There is already switch defined, we need to match it by hardware
         * info and we need to know that current switch VID also should match
         * since it's deterministic created.
         */

        auto sw = switches.begin()->second;

        /*
         * Switches VID must match, since it's deterministic.
         */

        if (switch_vid != sw->getVid())
        {
            SWSS_LOG_THROW("created switch VID don't match: previous %s, current: %s",
                    sai_serialize_object_id(switch_vid).c_str(),
                    sai_serialize_object_id(sw->getVid()).c_str());
        }

        /*
         * Also hardware info also must match.
         */

        std::string currentHw = sw->getHardwareInfo();
        std::string newHw;

        auto attr = sai_metadata_get_attr_by_id(SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO, attr_count, attr_list);

        if (attr == NULL)
        {
            /*
             * This is ok, attribute doesn't exist, so assumption is empty string.
             */
        }
        else
        {
            SWSS_LOG_DEBUG("new switch contains hardware info of length %u", attr->value.s8list.count);

            newHw = std::string((char*)attr->value.s8list.list, attr->value.s8list.count);
        }

        if (currentHw != newHw)
        {
            SWSS_LOG_THROW("hardware info missmatch: current '%s' vs new '%s'", currentHw.c_str(), newHw.c_str());
        }

        SWSS_LOG_NOTICE("current switch hardware info: '%s'", currentHw.c_str());
    }
    else
    {
        SWSS_LOG_THROW("number of switches is %zu in init view mode, this is not supported yet, FIXME", switches.size());
    }
}

sai_status_t processEventInInitViewMode(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since attributes are not checked, it may happen that user will send some
     * invalid VID in object id/list in attribute, metadata should handle that,
     * but if that happen, this id will be treated as "new" object instead of
     * existing one.
     */

    /*
     * TODO: use metadata utils.
     */

    auto info = sai_metadata_get_object_type_info(object_type);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:

            if (info->isnonobjectid)
            {
                /*
                 * We assume create of those non object id object types will succeed.
                 */
            }
            else
            {
                sai_object_id_t object_id;
                sai_deserialize_object_id(str_object_id, object_id);

                /*
                 * Object ID here is actual VID returned from redis during
                 * creation this is floating VID in init view mode.
                 */

                SWSS_LOG_DEBUG("generic create (init view) for %s, floating VID: %s",
                        sai_serialize_object_type(object_type).c_str(),
                        sai_serialize_object_id(object_id).c_str());

                if (object_type == SAI_OBJECT_TYPE_SWITCH)
                {
                    on_switch_create_in_init_view(object_id, attr_count, attr_list);
                }
            }

            return SAI_STATUS_SUCCESS;

        case SAI_COMMON_API_REMOVE:

            if (object_type == SAI_OBJECT_TYPE_SWITCH)
            {
                /*
                 * NOTE: Special care needs to be taken to clear all this
                 * switch id's from all db's currently we skip this since we
                 * assume that orchagent will not be removing just created
                 * switches. But it may happen when asic will fail etc.
                 *
                 * To support multiple switches this case must be refactored.
                 */

                SWSS_LOG_THROW("remove switch (%s) is not supported in init view mode yet! FIXME", str_object_id.c_str());
            }

            if (!info->isnonobjectid)
            {
                /*
                 * If object is existing object (like bridge port, vlan member)
                 * user may want to remove them, but this is temporary view,
                 * and when we receive apply view, we will populate existing
                 * objects to temporary view (since not all of them user may
                 * query) and this will produce conflict, since some of those
                 * objects user could explicitly remove. So to solve that we
                 * need to have a list of removed objects, and then only
                 * populate objects which not exist on removed list.
                 */

                sai_object_id_t object_vid;
                sai_deserialize_object_id(str_object_id, object_vid);

                initViewRemovedVidSet.insert(object_vid);
            }

            return SAI_STATUS_SUCCESS;

        case SAI_COMMON_API_SET:

            /*
             * We support SET api on all objects in init view mode.
             */

            return SAI_STATUS_SUCCESS;

        case SAI_COMMON_API_GET:

            {
                sai_status_t status;

                if (info->isnonobjectid)
                {
                    /*
                     * Those objects are user created, so if user created ROUTE he
                     * passed some attributes, there is no sense to support GET
                     * since user explicitly know what attributes were set, similar
                     * for other non object id types.
                     */

                    SWSS_LOG_ERROR("get is not supported on %s in init view mode", sai_serialize_object_type(object_type).c_str());

                    status = SAI_STATUS_NOT_SUPPORTED;
                }
                else
                {
                    sai_object_id_t object_id;
                    sai_deserialize_object_id(str_object_id, object_id);

                    SWSS_LOG_DEBUG("generic get (init view) for object type %s:%s",
                            sai_serialize_object_type(object_type).c_str(),
                            str_object_id.c_str());

                    /*
                     * Object must exists, we can't call GET on created object
                     * in init view mode, get here can be called on existing
                     * objects like default trap group to get some vendor
                     * specific values.
                     *
                     * Exception here is switch, since all switches must be
                     * created, when user will create switch on init view mode,
                     * switch will be matched with existing switch, or it will
                     * be explicitly created so user can query it properties.
                     *
                     * Translate vid to rid will make sure that object exist
                     * and it have RID defined, so we can query it.
                     */

                    sai_object_id_t rid = translate_vid_to_rid(object_id);

                    sai_object_meta_key_t meta_key;

                    meta_key.objecttype = object_type;
                    meta_key.objectkey.key.object_id = rid;

                    status = info->get(&meta_key, attr_count, attr_list);
                }

                sai_object_id_t switch_id;

                if (switches.size() == 1)
                {
                    /*
                     * We are in init view mode, but ether switch already
                     * existed or first command was creating switch and user
                     * created switch.
                     *
                     * We could change that later on, depends on object type we
                     * can extract switch id, we could also have this method
                     * inside metadata to get meta key.
                     */

                    switch_id = switches.begin()->second->getVid();
                }
                else
                {
                    /*
                     * This needs to be updated to support multiple switches
                     * scenario.
                     */

                    SWSS_LOG_THROW("multiple switches are not supported yet: %zu", switches.size());
                }

                internal_syncd_get_send(object_type, str_object_id, switch_id, status, attr_count, attr_list);

                return status;
            }

        default:

            SWSS_LOG_THROW("common api (%s) is not implemented in init view mode", sai_serialize_common_api(api).c_str());
    }

}

sai_object_id_t extractSwitchVid(
        _In_ sai_object_type_t object_type,
        _In_ const std::string& str_object_id)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(object_type);

    /*
     * Could be replaced by meta_key.
     */

    sai_fdb_entry_t fdb_entry;
    sai_neighbor_entry_t neighbor_entry;
    sai_route_entry_t route_entry;
    sai_object_id_t oid;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            sai_deserialize_fdb_entry(str_object_id, fdb_entry);
            return fdb_entry.switch_id;

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);
            return neighbor_entry.switch_id;

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            sai_deserialize_route_entry(str_object_id, route_entry);
            return route_entry.switch_id;

        default:

            if (info->isnonobjectid)
            {
                SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
            }

            sai_deserialize_object_id(str_object_id, oid);

            return redis_sai_switch_id_query(oid);
    }
}

sai_status_t handle_bulk_generic(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &object_ids,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>> &attributes)
{
    SWSS_LOG_ENTER();

    /*
     * Since we don't have asic support yet for bulk api, just execute one by
     * one.
     */

    for (size_t idx = 0; idx < object_ids.size(); ++idx)
    {
        sai_status_t status = SAI_STATUS_FAILURE;

        auto &list = attributes[idx];

        sai_attribute_t *attr_list = list->get_attr_list();
        uint32_t attr_count = list->get_attr_count();

        sai_object_meta_key_t meta_key;
        meta_key.objecttype = object_type;
        switch (object_type)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                sai_deserialize_route_entry(object_ids[idx], meta_key.objectkey.key.route_entry);
                break;
            case SAI_OBJECT_TYPE_FDB_ENTRY:
                sai_deserialize_fdb_entry(object_ids[idx], meta_key.objectkey.key.fdb_entry);
                break;
            case SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:
                sai_deserialize_object_id(object_ids[idx], meta_key.objectkey.key.object_id);
                break;
            default:
                SWSS_LOG_THROW("invalid object_type: %s", sai_serialize_object_type(object_type).c_str());
        }

        if (api == (sai_common_api_t)SAI_COMMON_API_BULK_SET)
        {
            status = handle_non_object_id(meta_key, SAI_COMMON_API_SET, attr_count, attr_list);
        }
        else if (api == (sai_common_api_t)SAI_COMMON_API_BULK_CREATE)
        {
            status = handle_non_object_id(meta_key, SAI_COMMON_API_CREATE, attr_count, attr_list);
        }
        else if (api == (sai_common_api_t)SAI_COMMON_API_BULK_REMOVE)
        {
            status = handle_non_object_id(meta_key, SAI_COMMON_API_REMOVE, attr_count, attr_list);
        }
        else
        {
            SWSS_LOG_ERROR("api %d is not supported in bulk route", api);
            exit_and_notify(EXIT_FAILURE);
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
    }

    return SAI_STATUS_SUCCESS;
}


sai_status_t processBulkEvent(
        _In_ sai_common_api_t api,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);

    std::string str_object_type = key.substr(0, key.find(":"));

    sai_object_type_t object_type;
    sai_deserialize_object_type(str_object_type, object_type);

    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    // key = str_object_id
    // val = attrid=attrvalue|...

    std::vector<std::string> object_ids;

    std::vector<std::shared_ptr<SaiAttributeList>> attributes;

    for (const auto &fvt: values)
    {
        std::string str_object_id = fvField(fvt);
        std::string joined = fvValue(fvt);

        // decode values

        auto v = swss::tokenize(joined, '|');

        object_ids.push_back(str_object_id);

        std::vector<swss::FieldValueTuple> entries; // attributes per object id

        for (size_t i = 0; i < v.size(); ++i)
        {
            const std::string item = v.at(i);

            auto start = item.find_first_of("=");

            auto field = item.substr(0, start);
            auto value = item.substr(start + 1);

            swss::FieldValueTuple entry(field, value);

            entries.push_back(entry);
        }

        // since now we converted this to proper list, we can extract attributes

        std::shared_ptr<SaiAttributeList> list =
            std::make_shared<SaiAttributeList>(object_type, entries, false);

        attributes.push_back(list);
    }

    SWSS_LOG_NOTICE("bulk %s execute with %zu items",
            str_object_type.c_str(),
            object_ids.size());

    if (isInitViewMode())
    {
        SWSS_LOG_ERROR("bulk api (%d) is not supported in init view mode", api);
        exit_and_notify(EXIT_FAILURE);
    }

    if (api != SAI_COMMON_API_BULK_GET)
    {
        // translate attributes for all objects

        for (auto &list: attributes)
        {
            sai_attribute_t *attr_list = list->get_attr_list();
            uint32_t attr_count = list->get_attr_count();

            translate_vid_to_rid_list(object_type, attr_count, attr_list);
        }
    }

    sai_status_t status;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            status = handle_bulk_generic(object_type, object_ids, api, attributes);
            break;

        default:
            SWSS_LOG_ERROR("bulk api for %s is not supported yet, FIXME",
                    sai_serialize_object_type(object_type).c_str());
            exit_and_notify(EXIT_FAILURE);
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to execute bulk api: %s",
                sai_serialize_status(status).c_str());

        exit_and_notify(EXIT_FAILURE);
    }

    return status;
}

sai_status_t processFdbFlush(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);
    const std::string &str_object_id = key.substr(key.find(":") + 1);

    sai_object_id_t switch_vid;

    sai_deserialize_object_id(str_object_id, switch_vid);

    sai_object_id_t switch_rid = translate_vid_to_rid(switch_vid);

    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    for (const auto &v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    SaiAttributeList list(SAI_OBJECT_TYPE_FDB_FLUSH, values, false);

    /*
     * Attribute list can't be const since we will use it to translate VID to
     * RID in place.
     */

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    translate_vid_to_rid_list(SAI_OBJECT_TYPE_FDB_FLUSH, attr_count, attr_list);

    sai_status_t status = sai_metadata_sai_fdb_api->flush_fdb_entries(switch_rid, attr_count, attr_list);

    std::vector<swss::FieldValueTuple> en;

    getResponse->set(sai_serialize_status(status), en, "flushresponse");

    return status;
}

sai_status_t processEvent(
        _In_ swss::ConsumerTable &consumer)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_mutex);

    swss::KeyOpFieldsValuesTuple kco;

    sai_status_t status = SAI_STATUS_SUCCESS;

    do {

        if (isInitViewMode())
        {
            /*
             * In init mode we put all data to TEMP view and we snoop.  We need to
             * specify temporary view prefix in consumer since consumer puts data
             * to redis db.
             */

            consumer.pop(kco, TEMP_PREFIX);
        }
        else
        {
            consumer.pop(kco);
        }

        const std::string &key = kfvKey(kco);
        const std::string &op = kfvOp(kco);

        if (key.length() == 0) {
            SWSS_LOG_DEBUG("no elements in m_buffer");
            return status;
        }

        /*
         * TODO: Key is serialized meta_key, we could use deserialize
         * to extract it here.
         */

        const std::string &str_object_type = key.substr(0, key.find(":"));
        const std::string &str_object_id = key.substr(key.find(":") + 1);

        SWSS_LOG_INFO("key: %s op: %s", key.c_str(), op.c_str());

        sai_common_api_t api = SAI_COMMON_API_MAX;

        if (op == "create")
        {
            api = SAI_COMMON_API_CREATE;
        }
        else if (op == "remove")
        {
            api = SAI_COMMON_API_REMOVE;
        }
        else if (op == "set")
        {
            api = SAI_COMMON_API_SET;
        }
        else if (op == "get")
        {
            api = SAI_COMMON_API_GET;
        }
        else if (op == "bulkset")
        {
            return processBulkEvent((sai_common_api_t)SAI_COMMON_API_BULK_SET, kco);
        }
        else if (op == "bulkcreate")
        {
            return processBulkEvent((sai_common_api_t)SAI_COMMON_API_BULK_CREATE, kco);
        }
        else if (op == "notify")
        {
            return notifySyncd(key);
        }
        else if (op == "get_stats")
        {
            return processGetStatsEvent(kco);
        }
        else if (op == "flush")
        {
            return processFdbFlush(kco);
        }
        else
        {
            SWSS_LOG_THROW("api '%s' is not implemented", op.c_str());
        }

        sai_object_type_t object_type;
        sai_deserialize_object_type(str_object_type, object_type);

        /*
         * TODO: use metadata utils is object type valid.
         */

        if (object_type == SAI_OBJECT_TYPE_NULL || object_type >= SAI_OBJECT_TYPE_EXTENSIONS_MAX)
        {
            SWSS_LOG_THROW("undefined object type %s", sai_serialize_object_type(object_type).c_str());
        }

        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        for (const auto &v: values)
        {
            SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
        }

        SaiAttributeList list(object_type, values, false);

        /*
         * Attribute list can't be const since we will use it to translate VID to
         * RID in place.
         */

        sai_attribute_t *attr_list = list.get_attr_list();
        uint32_t attr_count = list.get_attr_count();

        /*
         * NOTE: This check pointers must be executed before init view mode, since
         * this methods replaces pointers from orchagent memory space to syncd
         * memory space.
         */

        if (object_type == SAI_OBJECT_TYPE_SWITCH && (api == SAI_COMMON_API_CREATE || api == SAI_COMMON_API_SET))
        {
            /*
             * We don't need to clear those pointers on switch remove (even last),
             * since those pointers will reside inside attributes, also sairedis
             * will internally check whether pointer is null or not, so we here
             * will receive all notifications, but redis only those that were set.
             */

            check_notifications_pointers(attr_count, attr_list);
        }

        if (isInitViewMode())
        {
            return processEventInInitViewMode(object_type, str_object_id, api, attr_count, attr_list);
        }

        if (api != SAI_COMMON_API_GET)
        {
            /*
             * TODO we can also call translate on get, if sairedis will clean
             * buffer so then all OIDs will be NULL, and translation will also
             * convert them to NULL.
             */

            SWSS_LOG_DEBUG("translating VID to RIDs on all attributes");

            translate_vid_to_rid_list(object_type, attr_count, attr_list);
        }

        // TODO use metadata utils
        auto info = sai_metadata_get_object_type_info(object_type);

        /*
         * TODO use sai meta key deserialize
         */

        if (info->isnonobjectid)
        {
            sai_object_meta_key_t meta_key;

            meta_key.objecttype = object_type;

            switch (object_type)
            {
                case SAI_OBJECT_TYPE_FDB_ENTRY:
                    sai_deserialize_fdb_entry(str_object_id, meta_key.objectkey.key.fdb_entry);
                    break;

                case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                    sai_deserialize_neighbor_entry(str_object_id, meta_key.objectkey.key.neighbor_entry);
                    break;

                case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                    sai_deserialize_route_entry(str_object_id, meta_key.objectkey.key.route_entry);
                    break;

                default:

                    SWSS_LOG_THROW("non object id %s is not supported yet, FIXME", info->objecttypename);
            }

            status = handle_non_object_id(meta_key, api, attr_count, attr_list);
        }
        else
        {
            status = handle_generic(object_type, str_object_id, api, attr_count, attr_list);
        }

        if (api == SAI_COMMON_API_GET)
        {
            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_DEBUG("get API for key: %s op: %s returned status: %s",
                        key.c_str(),
                        op.c_str(),
                        sai_serialize_status(status).c_str());
            }

            /*
             * Extracting switch is double work here, we can avoid this when we
             * will use meta_key.
             */

            sai_object_id_t switch_vid = extractSwitchVid(object_type, str_object_id);

            internal_syncd_get_send(object_type, str_object_id, switch_vid, status, attr_count, attr_list);
        }
        else if (status != SAI_STATUS_SUCCESS)
        {
            if (!info->isnonobjectid && api == SAI_COMMON_API_SET)
            {
                sai_object_id_t vid;
                sai_deserialize_object_id(str_object_id, vid);

                sai_object_id_t rid = translate_vid_to_rid(vid);

                SWSS_LOG_ERROR("VID: %s RID: %s",
                        sai_serialize_object_id(vid).c_str(),
                        sai_serialize_object_id(rid).c_str());
            }

            for (const auto &v: values)
            {
                SWSS_LOG_ERROR("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
            }

            SWSS_LOG_THROW("failed to execute api: %s, key: %s, status: %s",
                    op.c_str(),
                    key.c_str(),
                    sai_serialize_status(status).c_str());
        }
    } while (!consumer.empty());

    return status;
}

void processFlexCounterGroupEvent(
        _In_ swss::ConsumerTable &consumer)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_mutex);
    swss::KeyOpFieldsValuesTuple kco;
    consumer.pop(kco);

    const auto &groupName = kfvKey(kco);
    const auto &op = kfvOp(kco);
    const auto values = kfvFieldsValues(kco);

    if (op == DEL_COMMAND)
    {
        FlexCounter::removeCounterPlugin(groupName);
        return;
    }

    for (const auto& valuePair : values)
    {
        const auto field = fvField(valuePair);
        const auto value = fvValue(valuePair);

        if (op == SET_COMMAND)
        {
            if (field == POLL_INTERVAL_FIELD)
            {
                FlexCounter::setPollInterval(stoi(value), groupName);
            }
            else if (field == QUEUE_PLUGIN_FIELD)
            {
                auto shaStrings = swss::tokenize(value, ',');
                for (const auto &sha : shaStrings)
                {
                    FlexCounter::addQueueCounterPlugin(sha, groupName);
                }
            }
            else if (field == PG_PLUGIN_FIELD)
            {
                auto shaStrings = swss::tokenize(value, ',');
                for (const auto &sha : shaStrings)
                {
                    FlexCounter::addPriorityGroupCounterPlugin(sha, groupName);
                }
            }
            else if (field == PORT_PLUGIN_FIELD)
            {
                auto shaStrings = swss::tokenize(value, ',');
                for (const auto &sha : shaStrings)
                {
                    FlexCounter::addPortCounterPlugin(sha, groupName);
                }
            }
            else if (field == FLEX_COUNTER_STATUS_FIELD)
            {
                FlexCounter::updateFlexCounterStatus(value, groupName);
            }
            else if (field == STATS_MODE_FIELD)
            {
                FlexCounter::updateFlexCounterStatsMode(value, groupName);
            }
            else
            {
                SWSS_LOG_ERROR("Field is not supported %s", field.c_str());
            }
        }
    }
}

void processFlexCounterEvent(
        _In_ swss::ConsumerTable &consumer)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_mutex);

    swss::KeyOpFieldsValuesTuple kco;
    consumer.pop(kco);

    const auto &key = kfvKey(kco);
    const auto &op = kfvOp(kco);

    std::size_t delimiter = key.find_first_of(":");
    if (delimiter == std::string::npos)
    {
        SWSS_LOG_ERROR("Failed to parse the key %s", key.c_str());
        return;
    }

    const auto groupName = key.substr(0, delimiter);
    const auto vidStr = key.substr(delimiter+1);

    sai_object_id_t vid = SAI_NULL_OBJECT_ID;
    sai_deserialize_object_id(vidStr, vid);
    sai_object_id_t rid = translate_vid_to_rid(vid);
    sai_object_type_t objectType = sai_object_type_query(rid);
    std::string  objectTypeStr = sai_serialize_object_type(objectType);

    if (op == DEL_COMMAND)
    {
        if (objectType == SAI_OBJECT_TYPE_PORT)
        {
            FlexCounter::removePort(vid, groupName);
        }
        else if (objectType == SAI_OBJECT_TYPE_QUEUE)
        {
            FlexCounter::removeQueue(vid, groupName);
        }
        else if (objectType == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP)
        {
            FlexCounter::removePriorityGroup(vid, groupName);
        }
        else if (objectType == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
        {
            FlexCounter::removeRif(vid, groupName);
        }
        else
        {
            SWSS_LOG_ERROR("Object type for removal not supported, %s", objectTypeStr.c_str());
        }
    }

    const auto values = kfvFieldsValues(kco);
    for (const auto& valuePair : values)
    {
        const auto field = fvField(valuePair);
        const auto value = fvValue(valuePair);

        if (op == SET_COMMAND)
        {
            auto idStrings  = swss::tokenize(value, ',');

            if (objectType == SAI_OBJECT_TYPE_PORT && field == PORT_COUNTER_ID_LIST)
            {
                std::vector<sai_port_stat_t> portCounterIds;
                for (const auto &str : idStrings)
                {
                    sai_port_stat_t stat;
                    sai_deserialize_port_stat(str.c_str(), &stat);
                    portCounterIds.push_back(stat);
                }
                FlexCounter::setPortCounterList(vid, rid, groupName, portCounterIds);
            }
            else if (objectType == SAI_OBJECT_TYPE_QUEUE && field == QUEUE_COUNTER_ID_LIST)
            {
                std::vector<sai_queue_stat_t> queueCounterIds;
                for (const auto &str : idStrings)
                {
                    sai_queue_stat_t stat;
                    sai_deserialize_queue_stat(str.c_str(), &stat);
                    queueCounterIds.push_back(stat);
                }
                FlexCounter::setQueueCounterList(vid, rid, groupName, queueCounterIds);
            }
            else if (objectType == SAI_OBJECT_TYPE_QUEUE && field == QUEUE_ATTR_ID_LIST)
            {
                std::vector<sai_queue_attr_t> queueAttrIds;
                for (const auto &str : idStrings)
                {
                    sai_queue_attr_t attr;
                    sai_deserialize_queue_attr(str, attr);
                    queueAttrIds.push_back(attr);
                }

                FlexCounter::setQueueAttrList(vid, rid, groupName, queueAttrIds);
            }
            else if (objectType == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP && field == PG_COUNTER_ID_LIST)
            {
                std::vector<sai_ingress_priority_group_stat_t> pgCounterIds;
                for (const auto &str : idStrings)
                {
                    sai_ingress_priority_group_stat_t stat;
                    sai_deserialize_ingress_priority_group_stat(str.c_str(), &stat);
                    pgCounterIds.push_back(stat);
                }
                FlexCounter::setPriorityGroupCounterList(vid, rid, groupName, pgCounterIds);
            }
            else if (objectType == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP && field == PG_ATTR_ID_LIST)
            {
                std::vector<sai_ingress_priority_group_attr_t> pgAttrIds;
                for (const auto &str : idStrings)
                {
                    sai_ingress_priority_group_attr_t attr;
                    sai_deserialize_ingress_priority_group_attr(str, attr);
                    pgAttrIds.push_back(attr);
                }

                FlexCounter::setPriorityGroupAttrList(vid, rid, groupName, pgAttrIds);
            }
            else if (objectType == SAI_OBJECT_TYPE_ROUTER_INTERFACE && field == RIF_COUNTER_ID_LIST)
            {
                std::vector<sai_router_interface_stat_t> rifCounterIds;
                for (const auto &str : idStrings)
                {
                    sai_router_interface_stat_t stat;
                    sai_deserialize_router_interface_stat(str.c_str(), &stat);
                    rifCounterIds.push_back(stat);
                }
                FlexCounter::setRifCounterList(vid, rid, groupName, rifCounterIds);
            }
            else
            {
                SWSS_LOG_ERROR("Object type and field combination is not supported, object type %s, field %s", objectTypeStr.c_str(), field.c_str());
            }
        }
    }
}

void printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: syncd [-N] [-U] [-d] [-p profile] [-i interval] [-t [cold|warm|fast|fastfast]] [-h] [-u] [-S]" << std::endl;
    std::cout << "    -N --nocounters" << std::endl;
    std::cout << "        Disable counter thread" << std::endl;
    std::cout << "    -d --diag" << std::endl;
    std::cout << "        Enable diagnostic shell" << std::endl;
    std::cout << "    -p --profile profile" << std::endl;
    std::cout << "        Provide profile map file" << std::endl;
    std::cout << "    -i --countersInterval interval" << std::endl;
    std::cout << "        Provide counter thread interval" << std::endl;
    std::cout << "    -t --startType type" << std::endl;
    std::cout << "        Specify cold|warm|fast|fastfast start type" << std::endl;
    std::cout << "    -u --useTempView:" << std::endl;
    std::cout << "        Use temporary view between init and apply" << std::endl;
    std::cout << "    -S --disableExitSleep" << std::endl;
    std::cout << "        Disable sleep when syncd crashes" << std::endl;
    std::cout << "    -U --eableUnittests" << std::endl;
    std::cout << "        Metadata enable unittests" << std::endl;
    std::cout << "    -C --eableConsistencyCheck" << std::endl;
    std::cout << "        Enable consisteny check DB vs ASIC after comparison logic" << std::endl;
#ifdef SAITHRIFT
    std::cout << "    -r --rpcserver"           << std::endl;
    std::cout << "        Enable rpcserver"      << std::endl;
    std::cout << "    -m --portmap"             << std::endl;
    std::cout << "        Specify port map file" << std::endl;
#endif // SAITHRIFT
    std::cout << "    -h --help" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}

void handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    options.disableExitSleep = false;
    options.enableUnittests = false;

#ifdef SAITHRIFT
    options.run_rpc_server = false;
    const char* const optstring = "dNUCt:p:i:rm:huS";
#else
    const char* const optstring = "dNUCt:p:i:huS";
#endif // SAITHRIFT

    while(true)
    {
        static struct option long_options[] =
        {
            { "useTempView",             no_argument,       0, 'u' },
            { "diag",                    no_argument,       0, 'd' },
            { "startType",               required_argument, 0, 't' },
            { "profile",                 required_argument, 0, 'p' },
            { "help",                    no_argument,       0, 'h' },
            { "disableExitSleep",        no_argument,       0, 'S' },
            { "enableUnittests",         no_argument,       0, 'U' },
            { "enableConsistencyCheck",  no_argument,       0, 'C' },
#ifdef SAITHRIFT
            { "rpcserver",               no_argument,       0, 'r' },
            { "portmap",                 required_argument, 0, 'm' },
#endif // SAITHRIFT
            { 0,                         0,                 0,  0  }
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'U':
                SWSS_LOG_NOTICE("enable unittests");
                options.enableUnittests = true;
                break;

            case 'C':
                SWSS_LOG_NOTICE("enable consistency check");
                g_enableConsistencyCheck = true;
                break;

            case 'u':
                SWSS_LOG_NOTICE("enable use temp view");
                options.useTempView = true;
                break;

            case 'S':
                SWSS_LOG_NOTICE("disable crash sleep");
                options.disableExitSleep = true;
                break;

            case 'd':
                SWSS_LOG_NOTICE("enable diag shell");
                options.diagShell = true;
                break;

            case 'p':
                SWSS_LOG_NOTICE("profile map file: %s", optarg);
                options.profileMapFile = std::string(optarg);
                break;

            case 't':
                SWSS_LOG_NOTICE("start type: %s", optarg);
                if (std::string(optarg) == "cold")
                {
                    options.startType = SAI_COLD_BOOT;
                }
                else if (std::string(optarg) == "warm")
                {
                    options.startType = SAI_WARM_BOOT;
                }
                else if (std::string(optarg) == "fast")
                {
                    options.startType = SAI_FAST_BOOT;
                }
                else if (std::string(optarg) == "fastfast")
                {
                    options.startType = SAI_FASTFAST_BOOT;
                }
                else
                {
                    SWSS_LOG_ERROR("unknown start type %s", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

#ifdef SAITHRIFT
            case 'r':
                SWSS_LOG_NOTICE("enable rpc server");
                options.run_rpc_server = true;
                break;
            case 'm':
                SWSS_LOG_NOTICE("port map file: %s", optarg);
                options.portMapFile = std::string(optarg);
                break;
#endif // SAITHRIFT

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }
}

void handleProfileMap(const std::string& profileMapFile)
{
    SWSS_LOG_ENTER();

    if (profileMapFile.size() == 0)
    {
        return;
    }

    std::ifstream profile(profileMapFile);

    if (!profile.is_open())
    {
        SWSS_LOG_ERROR("failed to open profile map file: %s : %s", profileMapFile.c_str(), strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Provide default value at boot up time and let sai profile value
    // Override following values if existing.
    // SAI reads these values at start up time. It would be too late to
    // set these values later when WARM BOOT is detected.
    gProfileMap[SAI_KEY_WARM_BOOT_WRITE_FILE] = DEF_SAI_WARM_BOOT_DATA_FILE;
    gProfileMap[SAI_KEY_WARM_BOOT_READ_FILE]  = DEF_SAI_WARM_BOOT_DATA_FILE;

    std::string line;

    while(getline(profile, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        size_t pos = line.find("=");

        if (pos == std::string::npos)
        {
            SWSS_LOG_WARN("not found '=' in line %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        gProfileMap[key] = value;

        SWSS_LOG_INFO("insert: %s:%s", key.c_str(), value.c_str());
    }
}

#ifdef SAITHRIFT
std::map<std::set<int>, std::string> gPortMap;

// FIXME: introduce common config format for SONiC
void handlePortMap(const std::string& portMapFile)
{
    SWSS_LOG_ENTER();

    if (portMapFile.size() == 0)
    {
        return;
    }

    std::ifstream portmap(portMapFile);

    if (!portmap.is_open())
    {
        std::cerr << "failed to open port map file:" << portMapFile.c_str() << " : "<< strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;

    while(getline(portmap, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        std::istringstream iss(line);
        std::string name, lanes, alias;
        iss >> name >> lanes >> alias;

        iss.clear();
        iss.str(lanes);
        std::string lane_str;
        std::set<int> lane_set;

        while (getline(iss, lane_str, ','))
        {
            int lane = stoi(lane_str);
            lane_set.insert(lane);
        }

        gPortMap.insert(std::pair<std::set<int>,std::string>(lane_set, name));
    }
}
#endif // SAITHRIFT

typedef enum _syncd_restart_type_t
{
    SYNCD_RESTART_TYPE_COLD,

    SYNCD_RESTART_TYPE_WARM,

    SYNCD_RESTART_TYPE_FAST,

    SYNCD_RESTART_TYPE_PRE_SHUTDOWN,

} syncd_restart_type_t;

syncd_restart_type_t handleRestartQuery(swss::NotificationConsumer &restartQuery)
{
    SWSS_LOG_ENTER();

    std::string op;
    std::string data;
    std::vector<swss::FieldValueTuple> values;

    restartQuery.pop(op, data, values);

    SWSS_LOG_DEBUG("op = %s", op.c_str());

    if (op == "COLD")
    {
        SWSS_LOG_NOTICE("received COLD switch shutdown event");
        return SYNCD_RESTART_TYPE_COLD;
    }

    if (op == "WARM")
    {
        SWSS_LOG_NOTICE("received WARM switch shutdown event");
        return SYNCD_RESTART_TYPE_WARM;
    }

    if (op == "FAST")
    {
        SWSS_LOG_NOTICE("received FAST switch shutdown event");
        return SYNCD_RESTART_TYPE_FAST;
    }

    if (op == "PRE-SHUTDOWN")
    {
        SWSS_LOG_NOTICE("received PRE_SHUTDOWN switch event");
        return SYNCD_RESTART_TYPE_PRE_SHUTDOWN;
    }

    SWSS_LOG_WARN("received '%s' unknown switch shutdown event, assuming COLD", op.c_str());
    return SYNCD_RESTART_TYPE_COLD;
}

bool isVeryFirstRun()
{
    SWSS_LOG_ENTER();

    /*
     * If lane map is not defined in redis db then we assume this is very first
     * start of syncd later on we can add additional checks here.
     *
     * TODO: if we add more switches then we need lane maps per switch.
     * TODO: we also need other way to check if this is first start
     *
     * We could use VIDCOUNTER also, but if something is defined in the DB then
     * we assume this is not the first start.
     *
     * TODO we need to fix this, since when there will be queue, it will still think
     * this is first run, let's query HIDDEN ?
     */

    auto keys = g_redisClient->keys(HIDDEN);

    bool firstRun = keys.size() == 0;

    SWSS_LOG_NOTICE("First Run: %s", firstRun ? "True" : "False");

    return firstRun;
}

int get_enum_value_from_name(
        _In_ const char *name,
        _In_ const sai_enum_metadata_t* metadata)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < metadata->valuescount; idx++)
    {
        if (strcmp(name, metadata->valuesnames[idx]) == 0)
        {
            return metadata->values[idx];
        }
    }

    SWSS_LOG_ERROR("not found %s", name);
    return 0;
}

void saiLoglevelNotify(std::string apiStr, std::string prioStr)
{
    SWSS_LOG_ENTER();

    using namespace swss;

    static const std::map<std::string, sai_log_level_t> saiLoglevelMap = {
        { "SAI_LOG_LEVEL_CRITICAL", SAI_LOG_LEVEL_CRITICAL },
        { "SAI_LOG_LEVEL_ERROR", SAI_LOG_LEVEL_ERROR },
        { "SAI_LOG_LEVEL_WARN", SAI_LOG_LEVEL_WARN },
        { "SAI_LOG_LEVEL_NOTICE", SAI_LOG_LEVEL_NOTICE },
        { "SAI_LOG_LEVEL_INFO", SAI_LOG_LEVEL_INFO },
        { "SAI_LOG_LEVEL_DEBUG", SAI_LOG_LEVEL_DEBUG },
    };

    if (saiLoglevelMap.find(prioStr) == saiLoglevelMap.end())
    {
        SWSS_LOG_ERROR("Invalid SAI loglevel %s %s", apiStr.c_str(), prioStr.c_str());
        return;
    }

    sai_api_t api = (sai_api_t)get_enum_value_from_name(apiStr.c_str(), &sai_metadata_enum_sai_api_t);

    sai_status_t status = sai_log_set(api, saiLoglevelMap.at(prioStr));

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_INFO("Failed to set %s on %s: %s", prioStr.c_str(), apiStr.c_str(),
                sai_serialize_status(status).c_str());
        return;
    }

    SWSS_LOG_NOTICE("Setting SAI loglevel %s to %s", apiStr.c_str(), prioStr.c_str());
}

void set_sai_api_loglevel()
{
    SWSS_LOG_ENTER();

    /*
     * We start from 1 since 0 is SAI_API_UNSPECIFIED.
     */

    for (uint32_t idx = 1; idx < sai_metadata_enum_sai_api_t.valuescount; ++idx)
    {
        swss::Logger::linkToDb(sai_metadata_enum_sai_api_t.valuesnames[idx], saiLoglevelNotify, "SAI_LOG_LEVEL_NOTICE");
    }
}

void set_sai_api_log_min_prio(const std::string &prioStr)
{
    SWSS_LOG_ENTER();

    /*
     * We start from 1 since 0 is SAI_API_UNSPECIFIED.
     */

    for (uint32_t idx = 1; idx < sai_metadata_enum_sai_api_t.valuescount; ++idx)
    {
        const auto& api_name = sai_metadata_enum_sai_api_t.valuesnames[idx];
        saiLoglevelNotify(api_name, prioStr);
    }
}

void onSyncdStart(bool warmStart)
{
    SWSS_LOG_ENTER();
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * It may happen that after initialize we will receive some port
     * notifications with port'ids that are not in redis db yet, so after
     * checking VIDTORID map there will be entries and translate_vid_to_rid
     * will generate new id's for ports, this may cause race condition so we
     * need to use a lock here to prevent that.
     */

    SWSS_LOG_TIMER("on syncd start");

    if (warmStart)
    {
        /*
         * Switch was warm started, so switches map is empty, we need to
         * recreate it based on existing entries inside database.
         *
         * Currently we expect only one switch, then we need to call it.
         *
         * Also this will make sure that current switch id is the same as
         * before restart.
         *
         * If we want to support multiple switches, this needs to be adjusted.
         */

        performWarmRestart();

        SWSS_LOG_NOTICE("skipping hard reinit since WARM start was performed");
        return;
    }

    SWSS_LOG_NOTICE("performing hard reinit since COLD start was performed");

    /*
     * Switch was restarted in hard way, we need to perform hard reinit and
     * recreate switches map.
     */

    hardReinit();
}

void sai_meta_log_syncd(
        _In_ sai_log_level_t log_level,
        _In_ const char *file,
        _In_ int line,
        _In_ const char *func,
        _In_ const char *format,
        ...)
    __attribute__ ((format (printf, 5, 6)));

void sai_meta_log_syncd(
        _In_ sai_log_level_t log_level,
        _In_ const char *file,
        _In_ int line,
        _In_ const char *func,
        _In_ const char *format,
        ...)
{
    // SWSS_LOG_ENTER() is omitted since this is logging for metadata

    char buffer[0x1000];

    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, 0x1000, format, ap);
    va_end(ap);

    swss::Logger::Priority p = swss::Logger::SWSS_NOTICE;

    switch (log_level)
    {
        case SAI_LOG_LEVEL_DEBUG:
            p = swss::Logger::SWSS_DEBUG;
            break;
        case SAI_LOG_LEVEL_INFO:
            p = swss::Logger::SWSS_INFO;
            break;
        case SAI_LOG_LEVEL_ERROR:
            p = swss::Logger::SWSS_ERROR;
            fprintf(stderr, "ERROR: %s: %s", func, buffer);
            break;
        case SAI_LOG_LEVEL_WARN:
            p = swss::Logger::SWSS_WARN;
            fprintf(stderr, "WARN: %s: %s", func, buffer);
            break;
        case SAI_LOG_LEVEL_CRITICAL:
            p = swss::Logger::SWSS_CRIT;
            break;

        default:
            p = swss::Logger::SWSS_NOTICE;
            break;
    }

    swss::Logger::getInstance().write(p, ":- %s: %s", func, buffer);
}

int syncd_main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    set_sai_api_loglevel();

    swss::Logger::linkToDbNative("syncd");

    swss::WarmStart::initialize("syncd", "syncd");
    swss::WarmStart::checkWarmStart("syncd", "syncd");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
    sai_metadata_log = &sai_meta_log_syncd;
#pragma GCC diagnostic pop

    meta_init_db();

    handleCmdLine(argc, argv);

    handleProfileMap(options.profileMapFile);

#ifdef SAITHRIFT
    if (options.portMapFile.size() > 0)
    {
        handlePortMap(options.portMapFile);
    }
#endif // SAITHRIFT

    std::shared_ptr<swss::DBConnector> dbAsic = std::make_shared<swss::DBConnector>(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    std::shared_ptr<swss::DBConnector> dbNtf = std::make_shared<swss::DBConnector>(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    std::shared_ptr<swss::DBConnector> dbFlexCounter = std::make_shared<swss::DBConnector>(FLEX_COUNTER_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    std::shared_ptr<swss::DBConnector> dbState = std::make_shared<swss::DBConnector>(STATE_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    std::unique_ptr<swss::Table> warmRestartTable = std::unique_ptr<swss::Table>(new swss::Table(dbState.get(), STATE_WARM_RESTART_TABLE_NAME));

    g_redisClient = std::make_shared<swss::RedisClient>(dbAsic.get());

    std::shared_ptr<swss::ConsumerTable> asicState = std::make_shared<swss::ConsumerTable>(dbAsic.get(), ASIC_STATE_TABLE);
    std::shared_ptr<swss::NotificationConsumer> restartQuery = std::make_shared<swss::NotificationConsumer>(dbAsic.get(), "RESTARTQUERY");
    std::shared_ptr<swss::ConsumerTable> flexCounter = std::make_shared<swss::ConsumerTable>(dbFlexCounter.get(), FLEX_COUNTER_TABLE);
    std::shared_ptr<swss::ConsumerTable> flexCounterGroup = std::make_shared<swss::ConsumerTable>(dbFlexCounter.get(), FLEX_COUNTER_GROUP_TABLE);

    /*
     * At the end we cant use producer consumer concept since if one process
     * will restart there may be something in the queue also "remove" from
     * response queue will also trigger another "response".
     */

    getResponse  = std::make_shared<swss::ProducerTable>(dbAsic.get(), "GETRESPONSE");
    notifications = std::make_shared<swss::NotificationProducer>(dbNtf.get(), "NOTIFICATIONS");

    g_veryFirstRun = isVeryFirstRun();

    /* ignore warm logic here if syncd starts in Mellanox fastfast boot mode */
    if (swss::WarmStart::isWarmStart() && (options.startType != SAI_FASTFAST_BOOT))
    {
        options.startType = SAI_WARM_BOOT;
    }

    if (options.startType == SAI_WARM_BOOT)
    {
        const char *warmBootReadFile = profile_get_value(0, SAI_KEY_WARM_BOOT_READ_FILE);

        SWSS_LOG_NOTICE("using warmBootReadFile: '%s'", warmBootReadFile);

        if (warmBootReadFile == NULL || access(warmBootReadFile, F_OK) == -1)
        {
            SWSS_LOG_WARN("user requested warmStart but warmBootReadFile is not specified or not accesible, forcing cold start");

            options.startType = SAI_COLD_BOOT;
        }
    }

    if (options.startType == SAI_WARM_BOOT && g_veryFirstRun)
    {
        SWSS_LOG_WARN("warm start requested, but this is very first syncd start, forcing cold start");

        /*
         * We force cold start since if it's first run then redis db is not
         * complete so redis asic view will not reflect warm boot asic state,
         * if this happen then orch agent needs to be restarted as well to
         * repopulate asic view.
         */

        options.startType = SAI_COLD_BOOT;
    }

    if (options.startType == SAI_FASTFAST_BOOT)
    {
        /*
         * Mellanox SAI requires to pass SAI_WARM_BOOT as SAI_BOOT_KEY
         * to start 'fastfast'
         */
        gProfileMap[SAI_KEY_BOOT_TYPE] = std::to_string(SAI_WARM_BOOT);
    } else {
        gProfileMap[SAI_KEY_BOOT_TYPE] = std::to_string(options.startType);
    }

    sai_status_t status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("fail to sai_api_initialize: %d", status);
        return EXIT_FAILURE;
    }

    sai_apis_t apis;
    int failed = sai_metadata_apis_query(sai_api_query, &apis);

    if (failed > 0)
    {
        SWSS_LOG_NOTICE("sai_api_query failed for %d apis", failed);
    }

    /*
     * TODO: user should create switch from OA, so shell should be started only
     * after we create switch.
     */

#ifdef SAITHRIFT
    if (options.run_rpc_server)
    {
        start_sai_thrift_rpc_server(SWITCH_SAI_THRIFT_RPC_SERVER_PORT);
        SWSS_LOG_NOTICE("rpcserver started");
    }
#endif // SAITHRIFT

    SWSS_LOG_NOTICE("syncd started");

    syncd_restart_type_t shutdownType = SYNCD_RESTART_TYPE_COLD;

    sai_switch_api_t *sai_switch_api = NULL;
    sai_api_query(SAI_API_SWITCH, (void**)&sai_switch_api);

    try
    {
        SWSS_LOG_NOTICE("before onSyncdStart");
        onSyncdStart(options.startType == SAI_WARM_BOOT);
        SWSS_LOG_NOTICE("after onSyncdStart");

        // create notifications processing thread after we create_switch to
        // make sure, we have switch_id translated to VID before we start
        // processing possible quick fdb notifications, and pointer for
        // notification queue is created before we create switch
        startNotificationsProcessingThread();

        SWSS_LOG_NOTICE("syncd listening for events");

        std::shared_ptr<swss::Select> s = std::make_shared<swss::Select>();

        s->addSelectable(asicState.get());
        s->addSelectable(restartQuery.get());
        s->addSelectable(flexCounter.get());
        s->addSelectable(flexCounterGroup.get());

        SWSS_LOG_NOTICE("starting main loop");

        while(true)
        {
            swss::Selectable *sel = NULL;

            int result = s->select(&sel);

            if (sel == restartQuery.get())
            {
                /*
                 * This is actual a bad design, since selectable may pick up
                 * multiple events from the queue, and after restart those
                 * events will be forgotten since they were consumed already and
                 * this may lead to forget populate object table which will
                 * lead to unable to find some objects.
                 */

                SWSS_LOG_NOTICE("is asic queue empty: %d",asicState->empty());

                while (!asicState->empty())
                {
                    processEvent(*asicState.get());
                }

                SWSS_LOG_NOTICE("drained queue");

                shutdownType = handleRestartQuery(*restartQuery);
                if (shutdownType != SYNCD_RESTART_TYPE_PRE_SHUTDOWN)
                {
                    // break out the event handling loop to shutdown syncd
                    break;
                }

                // Handle switch pre-shutdown and wait for the final shutdown
                // event

                SWSS_LOG_TIMER("warm pre-shutdown");

                FlexCounter::removeAllCounters();

                sai_attribute_t attr;

                attr.id = SAI_SWITCH_ATTR_RESTART_WARM;
                attr.value.booldata = true;

                status = sai_switch_api->set_switch_attribute(gSwitchId, &attr);

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("Failed to set SAI_SWITCH_ATTR_RESTART_WARM=true: %s for pre-shutdown",
                            sai_serialize_status(status).c_str());

                    shutdownType = SYNCD_RESTART_TYPE_COLD;

                    warmRestartTable->hset("warm-shutdown", "state", "set-flag-failed");
                    continue;
                }

                attr.id = SAI_SWITCH_ATTR_PRE_SHUTDOWN;
                attr.value.booldata = true;

                status = sai_switch_api->set_switch_attribute(gSwitchId, &attr);

                if (status == SAI_STATUS_SUCCESS)
                {
                    warmRestartTable->hset("warm-shutdown", "state", "pre-shutdown-succeeded");

                    s = std::make_shared<swss::Select>(); // make sure previous select is destroyed

                    s->addSelectable(restartQuery.get());

                    SWSS_LOG_NOTICE("switched to PRE_SHUTDOWN, from now on accepting only shurdown requests");
                }
                else
                {
                    SWSS_LOG_ERROR("Failed to set SAI_SWITCH_ATTR_PRE_SHUTDOWN=true: %s",
                            sai_serialize_status(status).c_str());

                    warmRestartTable->hset("warm-shutdown", "state", "pre-shutdown-failed");

                    // Restore cold shutdown.
                    attr.id = SAI_SWITCH_ATTR_RESTART_WARM;
                    attr.value.booldata = false;
                    status = sai_switch_api->set_switch_attribute(gSwitchId, &attr);
                }
            }
            else if (sel == flexCounter.get())
            {
                processFlexCounterEvent(*(swss::ConsumerTable*)sel);
            }
            else if (sel == flexCounterGroup.get())
            {
                processFlexCounterGroupEvent(*(swss::ConsumerTable*)sel);
            }
            else if (result == swss::Select::OBJECT)
            {
                processEvent(*(swss::ConsumerTable*)sel);
            }
        }
    }
    catch(const std::exception &e)
    {
        SWSS_LOG_ERROR("Runtime error: %s", e.what());

        exit_and_notify(EXIT_FAILURE);
    }

    if (shutdownType == SYNCD_RESTART_TYPE_WARM)
    {
        const char *warmBootWriteFile = profile_get_value(0, SAI_KEY_WARM_BOOT_WRITE_FILE);

        SWSS_LOG_NOTICE("using warmBootWriteFile: '%s'", warmBootWriteFile);

        if (warmBootWriteFile == NULL)
        {
            SWSS_LOG_WARN("user requested warm shutdown but warmBootWriteFile is not specified, forcing cold shutdown");

            shutdownType = SYNCD_RESTART_TYPE_COLD;
            warmRestartTable->hset("warm-shutdown", "state", "warm-shutdown-failed");
        }
        else
        {
            SWSS_LOG_NOTICE("Warm Reboot requested, keeping data plane running");

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_RESTART_WARM;
            attr.value.booldata = true;

            status = sai_switch_api->set_switch_attribute(gSwitchId, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("Failed to set SAI_SWITCH_ATTR_RESTART_WARM=true: %s, fall back to cold restart",
                        sai_serialize_status(status).c_str());
                shutdownType = SYNCD_RESTART_TYPE_COLD;
                warmRestartTable->hset("warm-shutdown", "state", "set-flag-failed");
            }
        }
    }

    SWSS_LOG_NOTICE("Removing the switch gSwitchId=0x%lx", gSwitchId);

#ifdef SAI_SUPPORT_UNINIT_DATA_PLANE_ON_REMOVAL

    if (shutdownType == SYNCD_RESTART_TYPE_FAST || shutdownType == SYNCD_RESTART_TYPE_WARM)
    {
        SWSS_LOG_NOTICE("Fast/warm reboot requested, keeping data plane running");

        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_UNINIT_DATA_PLANE_ON_REMOVAL;
        attr.value.booldata = false;

        status = sai_switch_api->set_switch_attribute(gSwitchId, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to set SAI_SWITCH_ATTR_UNINIT_DATA_PLANE_ON_REMOVAL=false: %s",
                    sai_serialize_status(status).c_str());
        }
    }

#endif

    FlexCounter::removeAllCounters();

    {
        SWSS_LOG_TIMER("remove switch");
        status = sai_switch_api->remove_switch(gSwitchId);
    }

    // Stop notification thread after removing switch
    stopNotificationsProcessingThread();

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_NOTICE("Can't delete a switch. gSwitchId=0x%lx status=%s", gSwitchId,
                sai_serialize_status(status).c_str());
    }

    if (shutdownType == SYNCD_RESTART_TYPE_WARM)
    {
        warmRestartTable->hset("warm-shutdown", "state",
                              (status == SAI_STATUS_SUCCESS) ?
                              "warm-shutdown-succeeded":
                              "warm-shutdown-failed");
    }

    SWSS_LOG_NOTICE("calling api uninitialize");

    status = sai_api_uninitialize();

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to uninitialize api: %s", sai_serialize_status(status).c_str());
    }

    SWSS_LOG_NOTICE("uninitialize finished");

    return EXIT_SUCCESS;
}
