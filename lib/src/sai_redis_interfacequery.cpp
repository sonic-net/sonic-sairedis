#include "sai_redis.h"
#include "sairedis.h"
#include "sairediscommon.h"

#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

#include "swss/selectableevent.h"
#include <string.h>

sai_service_method_table_t g_services;
bool                   g_apiInitialized = false;
volatile bool          g_run = false;

// this event is used to nice end notifications thread
swss::SelectableEvent g_redisNotificationThreadEvent;
std::shared_ptr<std::thread> notification_thread;

std::shared_ptr<swss::DBConnector>          g_db;
std::shared_ptr<swss::DBConnector>          g_dbNtf;
std::shared_ptr<swss::ProducerTable>        g_asicState;
std::shared_ptr<swss::ConsumerTable>        g_redisGetConsumer;
std::shared_ptr<swss::NotificationConsumer> g_redisNotifications;
std::shared_ptr<swss::RedisClient>          g_redisClient;
std::shared_ptr<swss::RedisPipeline>        g_redisPipeline;

void clear_local_state()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing local state");

    /*
     * Will need to be executed after init VIEW
     */

    /*
     * Clear current notifications pointers.
     *
     * Clear notifications before clearing metadata database to not cause
     * potential race condition for updating db right after clear.
     */

    clear_notifications();

    /*
     * Initialize metadata database.
     */

    meta_init_db();

    /*
     * Reset used switch ids.
     */

    redis_clear_switch_ids();
}

void ntf_thread()
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(g_redisNotifications.get());
    s.addSelectable(&g_redisNotificationThreadEvent);

    while (g_run)
    {
        swss::Selectable *sel;

        int result = s.select(&sel);

        if (sel == &g_redisNotificationThreadEvent)
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

            g_redisNotifications->pop(op, data, values);

            SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

            handle_notification(op, data, values);
        }
    }
}

sai_status_t sai_api_initialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t* services)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (g_apiInitialized)
    {
        SWSS_LOG_ERROR("api already initialized");

        return SAI_STATUS_FAILURE;
    }

    if ((NULL == services) || (NULL == services->profile_get_next_value) || (NULL == services->profile_get_value))
    {
        SWSS_LOG_ERROR("Invalid services handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&g_services, services, sizeof(g_services));

    g_db                 = std::make_shared<swss::DBConnector>(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    g_dbNtf              = std::make_shared<swss::DBConnector>(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    g_redisPipeline      = std::make_shared<swss::RedisPipeline>(g_db.get()); //enable default pipeline 128
    g_asicState          = std::make_shared<swss::ProducerTable>(g_redisPipeline.get(), ASIC_STATE_TABLE, true);
    g_redisGetConsumer   = std::make_shared<swss::ConsumerTable>(g_db.get(), "GETRESPONSE");
    g_redisNotifications = std::make_shared<swss::NotificationConsumer>(g_dbNtf.get(), "NOTIFICATIONS");
    g_redisClient        = std::make_shared<swss::RedisClient>(g_db.get());

    clear_local_state();

    g_asicInitViewMode = false;

    g_useTempView = false;

    g_run = true;

    setRecording(g_record);

    SWSS_LOG_DEBUG("creating notification thread");

    notification_thread = std::make_shared<std::thread>(ntf_thread);

    g_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_api_uninitialize(void)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (!g_apiInitialized)
    {
        SWSS_LOG_ERROR("api not initialized");

        return SAI_STATUS_FAILURE;
    }

    clear_local_state();

    g_run = false;

    // notify thread that it should end
    g_redisNotificationThreadEvent.notify();

    notification_thread->join();

    g_apiInitialized = false;

    return SAI_STATUS_SUCCESS;
}

// FIXME: Currently per API log level cannot be set
sai_status_t sai_log_set(
        _In_ sai_api_t sai_api_id,
        _In_ sai_log_level_t log_level)
{
    MUTEX();

    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

#define API_CASE(API,api)\
    case SAI_API_ ## API: {\
        *(const sai_ ## api ## _api_t**)api_method_table = &redis_ ## api ## _api;\
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

    if (!g_apiInitialized)
    {
        SWSS_LOG_ERROR("SAI API not initialized before calling API query");
        return SAI_STATUS_UNINITIALIZED;
    }

    switch (sai_api_id)
    {
        API_CASE(ACL,acl);
        API_CASE(BFD,bfd);
        API_CASE(BMTOR,bmtor);
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
        API_CASE(UDF,udf);
        API_CASE(VIRTUAL_ROUTER,virtual_router);
        API_CASE(VLAN,vlan);
        API_CASE(WRED,wred);
        API_CASE(DEBUG_COUNTER,debug_counter);
        API_CASE(NAT,nat);

        default:
            SWSS_LOG_ERROR("Invalid API type %d", sai_api_id);
            return SAI_STATUS_INVALID_PARAMETER;
    }
}

sai_status_t sai_query_attribute_enum_values_capability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();

    SWSS_LOG_ENTER();

    const std::string switch_id_str = sai_serialize_object_id(switch_id);
    const std::string object_type_str = sai_serialize_object_type(object_type);

    auto meta = sai_metadata_get_attr_metadata(object_type, attr_id);
    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d", object_type_str.c_str(), attr_id);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    const std::string attr_id_str = sai_serialize_attr_id(*meta);
    const std::string list_size = std::to_string(enum_values_capability->count);

    const std::vector<swss::FieldValueTuple> query_arguments =
    {
        swss::FieldValueTuple("OBJECT_TYPE", object_type_str),
        swss::FieldValueTuple("ATTR_ID", attr_id_str),
        swss::FieldValueTuple("LIST_SIZE", list_size)
    };

    SWSS_LOG_DEBUG(
            "Query arguments: switch %s, object type: %s, attribute: %s, count: %s",
            switch_id_str.c_str(),
            object_type_str.c_str(),
            attr_id_str.c_str(),
            list_size.c_str()
    );

    if (g_record)
    {
        recordLine("q|attribute_enum_values_capability|" + switch_id_str + "|" + joinFieldValues(query_arguments));
    }

    // This query will not put any data into the ASIC view, just into the
    // message queue
    g_asicState->set(switch_id_str, query_arguments, STRING_ATTR_ENUM_VALUES_CAPABILITY_QUERY);

    swss::Select callback;
    callback.addSelectable(g_redisGetConsumer.get());

    while (true)
    {
        SWSS_LOG_DEBUG("Waiting for a response");

        swss::Selectable *sel;

        auto result = callback.select(&sel, GET_RESPONSE_TIMEOUT);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisGetConsumer->pop(kco);

            const std::string &message_type = kfvOp(kco);
            const std::string &status_str = kfvKey(kco);

            SWSS_LOG_DEBUG("Received response: op = %s, key = %s", message_type.c_str(), status_str.c_str());

            // Ignore messages that are not in response to our query
            if (message_type != STRING_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE)
            {
                continue;
            }

            sai_status_t status;
            sai_deserialize_status(status_str, status);

            if (status == SAI_STATUS_SUCCESS)
            {
                const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

                if (values.size() != 2)
                {
                    if (g_record)
                    {
                        recordLine("Q|attribute_enum_values_capability|SAI_STATUS_FAILURE");
                    }

                    SWSS_LOG_ERROR("Invalid response from syncd: expected 2 values, received %zu", values.size());
                    return SAI_STATUS_FAILURE;
                }

                const std::string &capability_str = fvValue(values[0]);
                const uint32_t num_capabilities = std::stoi(fvValue(values[1]));

                SWSS_LOG_DEBUG("Received payload: capabilites = '%s', count = %d", capability_str.c_str(), num_capabilities);

                enum_values_capability->count = num_capabilities;

                size_t position = 0;
                for (uint32_t i = 0; i < num_capabilities; i++)
                {
                    size_t old_position = position;
                    position = capability_str.find(",", old_position);
                    std::string capability = capability_str.substr(old_position, position - old_position);
                    enum_values_capability->list[i] = std::stoi(capability);

                    // We have run out of values to add to our list
                    if (position == std::string::npos)
                    {
                        if (num_capabilities != i + 1)
                        {
                            SWSS_LOG_WARN("Query returned less attributes than expected: expected %d, recieved %d", num_capabilities, i+1);
                        }

                        break;
                    }

                    // Skip the commas
                    position++;
                }

                if (g_record)
                {
                    recordLine("Q|attribute_enum_values_capability|" + status_str + "|" + joinFieldValues(values));
                }
            }
            else
            {
                if (g_record)
                {
                    recordLine("Q|attribute_enum_values_capability|" + status_str);
                }
            }

            SWSS_LOG_DEBUG("Status: %s", status_str.c_str());
            return status;
        }
    }

    if (g_record)
    {
        recordLine("Q|attribute_enum_values_capability|SAI_STATUS_FAILURE");
    }

    SWSS_LOG_ERROR("Failed to receive a response from syncd");
    return SAI_STATUS_FAILURE;
}

sai_status_t sai_object_type_get_availability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _Out_ uint64_t *count)
{
    MUTEX();

    SWSS_LOG_ENTER();

    const std::string switch_id_str = sai_serialize_object_id(switch_id);
    const std::string object_type_str = sai_serialize_object_type(object_type);
    std::vector<swss::FieldValueTuple> query_arguments = SaiAttributeList::serialize_attr_list(
        object_type,
        attr_count,
        attr_list,
        false);

    SWSS_LOG_DEBUG(
            "Query arguments: switch: %s, object type: %s, attributes: %s",
            switch_id_str.c_str(),
            object_type_str.c_str(),
            joinFieldValues(query_arguments).c_str()
    );

    // Syncd will pop this argument off before trying to deserialize the attribute list
    query_arguments.push_back(swss::FieldValueTuple("OBJECT_TYPE", object_type_str));

    if (g_record)
    {
        recordLine("q|object_type_get_availability|" + switch_id_str + "|" + joinFieldValues(query_arguments));
    }

    // This query will not put any data into the ASIC view, just into the
    // message queue
    g_asicState->set(switch_id_str, query_arguments, STRING_OBJECT_TYPE_GET_AVAILABILITY_QUERY);

    swss::Select callback;
    callback.addSelectable(g_redisGetConsumer.get());

    while (true)
    {
        SWSS_LOG_DEBUG("Waiting for a response");

        swss::Selectable *sel;

        auto result = callback.select(&sel, GET_RESPONSE_TIMEOUT);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisGetConsumer->pop(kco);

            const std::string &message_type = kfvOp(kco);
            const std::string &status_str = kfvKey(kco);

            SWSS_LOG_DEBUG("Received response: op = %s, key = %s", message_type.c_str(), status_str.c_str());

            // Ignore messages that are not in response to our query
            if (message_type != STRING_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE)
            {
                continue;
            }

            sai_status_t status;
            sai_deserialize_status(status_str, status);

            if (status == SAI_STATUS_SUCCESS)
            {
                const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

                if (values.size() != 1)
                {
                    if (g_record)
                    {
                        recordLine("Q|object_type_get_availability|SAI_STATUS_FAILURE");
                    }

                    SWSS_LOG_ERROR("Invalid response from syncd: expected 1 value, received %zu", values.size());
                    return SAI_STATUS_FAILURE;
                }

                const std::string &availability_str = fvValue(values[0]);
                *count = std::stol(availability_str);

                SWSS_LOG_DEBUG("Received payload: count = %lu", *count);

                if (g_record)
                {
                    recordLine("Q|object_type_get_availability|" + status_str + "|" + joinFieldValues(values));
                }
            }
            else
            {
                if (g_record)
                {
                    recordLine("Q|object_type_get_availability|" + status_str);
                }
            }

            SWSS_LOG_DEBUG("Status: %s", status_str.c_str());
            return status;
        }
    }

    if (g_record)
    {
        recordLine("Q|object_type_get_availability|SAI_STATUS_FAILURE");
    }

    SWSS_LOG_ERROR("Failed to receive a response from syncd");
    return SAI_STATUS_FAILURE;
}
