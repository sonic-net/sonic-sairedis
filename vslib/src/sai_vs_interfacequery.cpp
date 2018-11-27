#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include <string.h>
#include <unistd.h>
#include <net/if.h>

#include <algorithm>

#include "swss/notificationconsumer.h"
#include "swss/select.h"

bool                    g_api_initialized = false;
bool                    g_vs_hostif_use_tap_device = false;
sai_vs_switch_type_t    g_vs_switch_type = SAI_VS_SWITCH_TYPE_NONE;
std::recursive_mutex    g_recursive_mutex;

volatile bool                               g_unittestChannelRun;
swss::SelectableEvent                       g_unittestChannelThreadEvent;
std::shared_ptr<std::thread>                g_unittestChannelThread;
std::shared_ptr<swss::NotificationConsumer> g_unittestChannelNotificationConsumer;
std::shared_ptr<swss::DBConnector>          g_dbNtf;

volatile bool                               g_fdbAgingThreadRun;
std::shared_ptr<std::thread>                g_fdbAgingThread;

int g_vs_boot_type = SAI_VS_COLD_BOOT;

std::map<uint32_t,std::string> g_lane_to_ifname;
std::map<std::string,std::vector<uint32_t>> g_ifname_to_lanes;
std::vector<uint32_t> g_lane_order;

const char *g_boot_type             = NULL;
const char *g_warm_boot_read_file   = NULL;
const char *g_warm_boot_write_file  = NULL;

const char *g_interface_lane_map_file = NULL;

void channelOpEnableUnittests(
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    bool enable = (key == "true");

    meta_unittests_enable(enable);
}

void channelOpSetReadOnlyAttribute(
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    for (const auto &v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    if (values.size() != 1)
    {
        SWSS_LOG_ERROR("expected 1 value only, but given: %zu", values.size());
        return;
    }

    const std::string &str_object_type = key.substr(0, key.find(":"));
    const std::string &str_object_id = key.substr(key.find(":") + 1);

    sai_object_type_t object_type;
    sai_deserialize_object_type(str_object_type, object_type);

    if (object_type == SAI_OBJECT_TYPE_NULL || object_type >= SAI_OBJECT_TYPE_MAX)
    {
        SWSS_LOG_ERROR("invalid object type: %d", object_type);
        return;
    }

    auto info = sai_metadata_get_object_type_info(object_type);

    if (info->isnonobjectid)
    {
        SWSS_LOG_ERROR("non object id %s is not supported yet", str_object_type.c_str());
        return;
    }

    sai_object_id_t object_id;

    sai_deserialize_object_id(str_object_id, object_id);

    sai_object_type_t ot = sai_object_type_query(object_id);

    if (ot != object_type)
    {
        SWSS_LOG_ERROR("object type is differnt than provided %s, but oid is %s",
                str_object_type.c_str(), sai_serialize_object_type(ot).c_str());
        return;
    }

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to find switch id for oid %s", str_object_id.c_str());
        return;
    }

    // oid is validated and we got switch id

    const std::string &str_attr_id = fvField(values.at(0));
    const std::string &str_attr_value = fvValue(values.at(0));

    auto meta = sai_metadata_get_attr_metadata_by_attr_id_name(str_attr_id.c_str());

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("failed to find attr %s", str_attr_id.c_str());
        return;
    }

    if (meta->objecttype != ot)
    {
        SWSS_LOG_ERROR("attr %s belongs to differnt object type than oid: %s",
                str_attr_id.c_str(), sai_serialize_object_type(ot).c_str());
        return;
    }

    // we got attr metadata

    sai_attribute_t attr;

    attr.id = meta->attrid;

    sai_deserialize_attr_value(str_attr_value, *meta, attr);

    SWSS_LOG_NOTICE("switch id is %s", sai_serialize_object_id(switch_id).c_str());

    sai_status_t status = meta_unittests_allow_readonly_set_once(meta->objecttype, meta->attrid);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to enable SET readonly attribute once: %s", sai_serialize_status(status).c_str());
        return;
    }

    sai_object_meta_key_t meta_key = { .objecttype = ot, .objectkey = { .key = { .object_id = object_id } } };

    status = info->set(&meta_key, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to set %s to %s on %s",
                str_attr_id.c_str(), str_attr_value.c_str(), str_object_id.c_str());
    }
    else
    {
        SWSS_LOG_NOTICE("SUCCESS to set %s to %s on %s",
                str_attr_id.c_str(), str_attr_value.c_str(), str_object_id.c_str());
    }

    sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
}

void channelOpSetStats(
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    // NOTE: we need to find stats for specific object, later SAI already have
    // this feature and this search could be optimized here:
    // https://github.com/opencomputeproject/SAI/commit/acc83933ff21c68e8ef10c9826de45807fdc0438

    sai_object_id_t oid;

    sai_deserialize_object_id(key, oid);

    sai_object_type_t ot = sai_object_type_query(oid);

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("invalid object id: %s", key.c_str());
        return;
    }

    sai_object_id_t switch_id = sai_switch_id_query(oid);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("unable to get switch_id from oid: %s", key.c_str());
        return;
    }

    /*
     * Check if object for statistics was created and exists on switch.
     */

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(ot);

    auto it = objectHash.find(key.c_str());

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("object not found: %s", key.c_str());
        return;
    }

    /*
     * Check if object for statistics have statistic map created, if not
     * create empty map.
     */

    auto &countersMap = g_switch_state_map.at(switch_id)->countersMap;

    auto mapit = countersMap.find(key);

    if (mapit == countersMap.end())
        countersMap[key] = std::map<int,uint64_t>();

    /*
     * Find stats enum based on object type.  In new metadata we have enum on
     * object type, but here we need to find it manually enum is in format
     * "sai_" + object_type + "_stat_t"
     */

    std::string lower_ot = sai_serialize_object_type(ot).substr(16);  // 16 = skip "SAI_OBJECT_TYPE_"

    std::transform(lower_ot.begin(), lower_ot.end(), lower_ot.begin(), ::tolower);

    std::string stat_enum_name = "sai_" + lower_ot + "_stat_t";

    const sai_enum_metadata_t* statenum = NULL;

    for (size_t i = 0; i < sai_metadata_all_enums_count; ++i)
    {
        if (sai_metadata_all_enums[i]->name == stat_enum_name)
        {
            SWSS_LOG_INFO("found enum %s", stat_enum_name.c_str());
            // found
            statenum = sai_metadata_all_enums[i];
            break;
        }
    }

    if (statenum == NULL)
    {
        SWSS_LOG_ERROR("failed to find stat enum: %s", stat_enum_name.c_str());
        return;
    }

    for (auto v: values)
    {
        // value format: stat_enum_name:uint64

        auto name = fvField(v);

        uint64_t value;

        if (sscanf(fvValue(v).c_str(), "%lu", &value) != 1)
        {
            SWSS_LOG_ERROR("failed to deserialize %s as couner value uint64_t", fvValue(v).c_str());
        }

        // linear search

        int enumvalue = -1;

        for (size_t i = 0; i < statenum->valuescount; ++i)
        {
            if (statenum->valuesnames[i] == name)
            {
                enumvalue = statenum->values[i];
                break;
            }
        }

        if (enumvalue == -1)
        {
            SWSS_LOG_ERROR("failed to find enum value: %s", name.c_str());
            continue;
        }

        SWSS_LOG_DEBUG("writting %s = %lu on %s", name.c_str(), value, key.c_str());

        countersMap.at(key)[enumvalue] = value;
    }
}

void handleUnittestChannelOp(
        _In_ const std::string &op,
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    MUTEX();

    SWSS_LOG_ENTER();

    /*
     * Since we will access and modify DB we need to be under mutex.
     *
     * NOTE: since this unittest channel is handled in thread, then that means
     * there is a DELAY from producer and consumer thread in VS, so if user
     * will set value on the specific READ_ONLY value he should wait for some
     * time until that value will be propagated to virtual switch.
     */

    SWSS_LOG_NOTICE("op = %s, key = %s", op.c_str(), key.c_str());

    for (const auto &v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    if (op == SAI_VS_UNITTEST_ENABLE_UNITTESTS)
    {
        channelOpEnableUnittests(key, values);
    }
    else if (op == SAI_VS_UNITTEST_SET_RO_OP)
    {
        channelOpSetReadOnlyAttribute(key, values);
    }
    else if (op == SAI_VS_UNITTEST_SET_STATS_OP)
    {
        channelOpSetStats(key, values);
    }
    else
    {
        SWSS_LOG_ERROR("unknown unittest operation: %s", op.c_str());
    }
}

void unittestChannelThreadProc()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("enter VS unittest channel thread");

    swss::Select s;

    s.addSelectable(g_unittestChannelNotificationConsumer.get());
    s.addSelectable(&g_unittestChannelThreadEvent);

    while (g_unittestChannelRun)
    {
        swss::Selectable *sel;

        int result = s.select(&sel);

        if (sel == &g_unittestChannelThreadEvent)
        {
            // user requested shutdown_switch
            break;
        }

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            std::string op;
            std::string data;
            std::vector<swss::FieldValueTuple> values;

            g_unittestChannelNotificationConsumer->pop(op, data, values);

            SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

            try
            {
                handleUnittestChannelOp(op, data, values);
            }
            catch (const std::exception &e)
            {
                SWSS_LOG_ERROR("Exception: op = %s, data = %s, %s", op.c_str(), data.c_str(), e.what());
            }
        }
    }

    SWSS_LOG_NOTICE("exit VS unittest channel thread");
}

void processFdbEntriesForAging()
{
    SWSS_LOG_ENTER();

    if (!g_recursive_mutex.try_lock())
    {
        return;
    }

    SWSS_LOG_INFO("fdb infos to process: %zu", g_fdb_info_set.size());

    uint32_t current = (uint32_t)time(NULL);

    // find aged fdb entries

    for (auto it = g_fdb_info_set.begin(); it != g_fdb_info_set.end();)
    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;

        sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_SWITCH, it->fdb_entry.switch_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("failed to get FDB aging time for switch %s",
                    sai_serialize_object_id(it->fdb_entry.switch_id).c_str());

            ++it;
            continue;
        }

        uint32_t aging_time = attr.value.u32;

        if (aging_time == 0)
        {
            // aging is disabled
            ++it;
            continue;
        }

        if ((current - it->timestamp) >= aging_time)
        {
            fdb_info_t fi = *it;

            processFdbInfo(fi, SAI_FDB_EVENT_AGED);

            it = g_fdb_info_set.erase(it);
        }
        else
        {
            ++it;
        }
    }

    g_recursive_mutex.unlock();
}

void fdbAgingThreadProc()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("starting fdb aging thread");

    while (g_fdbAgingThreadRun)
    {
        processFdbEntriesForAging();

        sleep(1);
    }

    SWSS_LOG_NOTICE("ending fdb aging thread");
}

/**
 * @brief Service method table.
 *
 * We could use this table to choose switch vendor.
 */
static sai_service_method_table_t g_service_method_table;

void clear_local_state()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing local state");

    /*
     * Initialize metadata database.
     */

    meta_init_db();

    /*
     * Reset used switch ids.
     */

    vs_clear_switch_ids();

    /*
     * Reset real counter id values
     */

    vs_reset_id_counter();
}

bool check_ifname(
        _In_ const std::string& name)
{
    SWSS_LOG_ENTER();

    size_t size = name.size();

    if (size == 0 || size > IFNAMSIZ)
    {
        SWSS_LOG_ERROR("invalid interface name %s length: %zu", name.c_str(), size);
        return false;
    }

    for (size_t i = 0; i < size; i++)
    {
        char c = name[i];

        if (c >= '0' && c <= '9')
            continue;

        if (c >= 'a' && c <= 'z')
            continue;

        if (c >= 'A' && c <= 'Z')
            continue;

        SWSS_LOG_ERROR("invalid character '%c' in interface name %s", c, name.c_str());
        return false;
    }

    // interface name is valid
    return true;
}

void load_interface_lane_map()
{
    SWSS_LOG_ENTER();

    if (g_interface_lane_map_file == NULL)
    {
        SWSS_LOG_NOTICE("no interface lane map");
        return;
    }

    std::ifstream lanemap(g_interface_lane_map_file);

    if (!lanemap.is_open())
    {
        SWSS_LOG_ERROR("failed to open lane map file: %s", g_interface_lane_map_file);
        return;
    }

    std::string line;

    while(getline(lanemap, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        auto tokens = swss::tokenize(line, ':');

        if (tokens.size() != 2)
        {
            SWSS_LOG_ERROR("expected 2 tokens in line %s, got %zu", line.c_str(), tokens.size());
            continue;
        }

        auto ifname = tokens.at(0);
        auto lanes = tokens.at(1);

        if (!check_ifname(ifname))
        {
            continue;
        }

        if (g_ifname_to_lanes.find(ifname) != g_ifname_to_lanes.end())
        {
            SWSS_LOG_ERROR("interface %s was already defined", ifname.c_str());
            continue;
        }

        tokens = swss::tokenize(lanes,',');

        size_t n = tokens.size();

        if (n != 1 && n != 2 && n != 4)
        {
            SWSS_LOG_ERROR("invalid number of lanes (%zu) assigned to interface %s", n, ifname.c_str());
            continue;
        }

        std::vector<uint32_t> lanevec;

        for (auto l: tokens)
        {
            uint32_t lanenumber;
            if (sscanf(l.c_str(), "%u", &lanenumber) != 1)
            {
                SWSS_LOG_ERROR("failed to parse lane number: %s", l.c_str());
                continue;
            }

            if (g_lane_to_ifname.find(lanenumber) != g_lane_to_ifname.end())
            {
                SWSS_LOG_ERROR("lane number %u used on %s was already defined on %s",
                        lanenumber,
                        ifname.c_str(),
                        g_lane_to_ifname.at(lanenumber).c_str());
                continue;
            }

            lanevec.push_back(lanenumber);
            g_lane_order.push_back(lanenumber);

            g_lane_to_ifname[lanenumber] = ifname;
        }

        g_ifname_to_lanes[ifname] = lanevec;
    }

    SWSS_LOG_NOTICE("loaded %zu lanes and %zu interfaces", g_lane_to_ifname.size(), g_ifname_to_lanes.size());
}

sai_status_t sai_api_initialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (g_api_initialized)
    {
        SWSS_LOG_ERROR("api already initialized");

        return SAI_STATUS_FAILURE;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&g_service_method_table, service_method_table, sizeof(g_service_method_table));

    // TODO maybe this query should be done right before switch create
    const char *type = service_method_table->profile_get_value(0, SAI_KEY_VS_SWITCH_TYPE);

    if (type == NULL)
    {
        SWSS_LOG_ERROR("failed to obtain service method table value: %s", SAI_KEY_VS_SWITCH_TYPE);

        return SAI_STATUS_FAILURE;
    }

    g_interface_lane_map_file = service_method_table->profile_get_value(0, SAI_KEY_VS_INTERFACE_LANE_MAP_FILE);

    load_interface_lane_map();

    g_boot_type             = service_method_table->profile_get_value(0, SAI_KEY_BOOT_TYPE);
    g_warm_boot_read_file   = service_method_table->profile_get_value(0, SAI_KEY_WARM_BOOT_READ_FILE);
    g_warm_boot_write_file  = service_method_table->profile_get_value(0, SAI_KEY_WARM_BOOT_WRITE_FILE);

    std::string bt = (g_boot_type == NULL) ? "cold" : g_boot_type;

    if (bt == "cold" || bt == "0")
    {
        g_vs_boot_type = SAI_VS_COLD_BOOT;
    }
    else if (bt == "warm" || bt == "1")
    {
        g_vs_boot_type = SAI_VS_WARM_BOOT;
    }
    else if (bt == "fast" || bt == "2")
    {
        g_vs_boot_type = SAI_VS_FAST_BOOT;
    }
    else
    {
        SWSS_LOG_ERROR("unsupported boot type: %s", g_boot_type);

        return SAI_STATUS_FAILURE;
    }

    std::string strType = type;

    if (strType == SAI_VALUE_VS_SWITCH_TYPE_BCM56850)
    {
        g_vs_switch_type = SAI_VS_SWITCH_TYPE_BCM56850;
    }
    else if (strType == SAI_VALUE_VS_SWITCH_TYPE_MLNX2700)
    {
        g_vs_switch_type = SAI_VS_SWITCH_TYPE_MLNX2700;
    }
    else
    {
        SWSS_LOG_ERROR("unknown switch type: '%s'", type);

        return SAI_STATUS_FAILURE;
    }

    const char *use_tap_dev = service_method_table->profile_get_value(0, SAI_KEY_VS_HOSTIF_USE_TAP_DEVICE);

    g_vs_hostif_use_tap_device = use_tap_dev != NULL && strcmp(use_tap_dev, "true") == 0;

    SWSS_LOG_NOTICE("hostif use TAP device: %s",
            g_vs_hostif_use_tap_device ? "true" : "false");

    if (flags != 0)
    {
        SWSS_LOG_ERROR("invalid flags passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    clear_local_state();

    g_dbNtf = std::make_shared<swss::DBConnector>(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    g_unittestChannelNotificationConsumer = std::make_shared<swss::NotificationConsumer>(g_dbNtf.get(), SAI_VS_UNITTEST_CHANNEL);

    g_unittestChannelRun = true;

    g_unittestChannelThread = std::make_shared<std::thread>(std::thread(unittestChannelThreadProc));

    g_fdb_info_set.clear();

    g_fdbAgingThreadRun = true;

    // TODO should this be moved to create switch and SwitchState?
    g_fdbAgingThread = std::make_shared<std::thread>(std::thread(fdbAgingThreadProc));

    g_api_initialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_api_uninitialize(void)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (!g_api_initialized)
    {
        SWSS_LOG_ERROR("api not initialized");

        return SAI_STATUS_FAILURE;
    }

    clear_local_state();

    g_unittestChannelRun = false;

    //// notify thread that it should end
    g_unittestChannelThreadEvent.notify();

    g_unittestChannelThread->join();

    g_fdbAgingThreadRun = false;

    g_fdbAgingThread->join();

    g_api_initialized = false;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_log_set(
        _In_ sai_api_t sai_api_id,
        _In_ sai_log_level_t log_level)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

#define API_CASE(API,api)                                                       \
    case SAI_API_ ## API: {                                                     \
        *(const sai_ ## api ## _api_t**)api_method_table = &vs_ ## api ## _api; \
        return SAI_STATUS_SUCCESS; }

sai_status_t sai_api_query(
        _In_ sai_api_t sai_api_id,
        _Out_ void** api_method_table)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (api_method_table == NULL)
    {
        SWSS_LOG_ERROR("NULL method table passed to SAI API initialize");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (!g_api_initialized)
    {
        SWSS_LOG_ERROR("SAI API not initialized before calling API query");
        return SAI_STATUS_UNINITIALIZED;
    }

    switch (sai_api_id)
    {
        API_CASE(ACL,acl);
        API_CASE(BFD,bfd);
        API_CASE(BRIDGE,bridge);
        API_CASE(BUFFER,buffer);
        API_CASE(DTEL,dtel);
        API_CASE(FDB,fdb);
        API_CASE(HASH,hash);
        API_CASE(HOSTIF,hostif);
        API_CASE(IPMC_GROUP,ipmc_group);
        API_CASE(IPMC,ipmc);
        //API_CASE(ISOLATION_GROUP,isolation_group);
        API_CASE(L2MC_GROUP,l2mc_group);
        API_CASE(L2MC,l2mc);
        API_CASE(LAG,lag);
        API_CASE(MCAST_FDB,mcast_fdb);
        API_CASE(MIRROR,mirror);
        API_CASE(MPLS,mpls);
        API_CASE(NEIGHBOR,neighbor);
        API_CASE(NEXT_HOP_GROUP,next_hop_group);
        API_CASE(NEXT_HOP,next_hop);
        API_CASE(POLICER,policer);
        API_CASE(PORT,port);
        API_CASE(QOS_MAP,qos_map);
        API_CASE(QUEUE,queue);
        API_CASE(ROUTER_INTERFACE,router_interface);
        API_CASE(ROUTE,route);
        API_CASE(RPF_GROUP,rpf_group);
        API_CASE(SAMPLEPACKET,samplepacket);
        API_CASE(SCHEDULER_GROUP,scheduler_group);
        API_CASE(SCHEDULER,scheduler);
        API_CASE(SEGMENTROUTE,segmentroute);
        API_CASE(STP,stp);
        API_CASE(SWITCH,switch);
        API_CASE(TAM,tam);
        API_CASE(TUNNEL,tunnel);
        API_CASE(UBURST,uburst);
        API_CASE(UDF,udf);
        API_CASE(VIRTUAL_ROUTER,virtual_router);
        API_CASE(VLAN,vlan);
        API_CASE(WRED,wred);

        default:
            SWSS_LOG_ERROR("Invalid API type %d", sai_api_id);
            return SAI_STATUS_INVALID_PARAMETER;
    }
}
