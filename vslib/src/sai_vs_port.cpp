#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_port(
            _Out_ sai_object_id_t *port_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    /* create port */
    CHECK_STATUS(meta_sai_create_oid((sai_object_type_t)SAI_OBJECT_TYPE_PORT,
                port_id,switch_id,attr_count,attr_list,&vs_generic_create));

    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    attr.value.booldata = false;     /* default admin state is down as defined in SAI */

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, *port_id, &attr));

    /* create priority groups */
    const uint32_t port_pgs_count = 8;

    std::vector<sai_object_id_t> pgs;

    for (uint32_t i = 0; i < port_pgs_count; ++i)
    {
        sai_object_id_t pg_id;

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, &pg_id, switch_id, 0, NULL));

        pgs.push_back(pg_id);
    }

    attr.id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;
    attr.value.u32 = port_pgs_count;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, *port_id, &attr));

    attr.id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
    attr.value.objlist.count = port_pgs_count;
    attr.value.objlist.list = pgs.data();

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, *port_id, &attr));

    /* create qos queues */
    const uint32_t port_qos_queues_count = 20;

    std::vector<sai_object_id_t> queues;

    for (uint32_t i = 0; i < port_qos_queues_count; ++i)
    {
        sai_object_id_t queue_id;

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_QUEUE, &queue_id, switch_id, 0, NULL));

        queues.push_back(queue_id);
    }

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    attr.value.u32 = port_qos_queues_count;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, *port_id, &attr));

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = port_qos_queues_count;
    attr.value.objlist.list = queues.data();

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, *port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

VS_REMOVE(PORT,port);
VS_SET(PORT,port);
VS_GET(PORT,port);
VS_GENERIC_QUAD(PORT_POOL,port_pool);
VS_GENERIC_STATS(PORT,port);
VS_GENERIC_STATS(PORT_POOL,port_pool);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)
    VS_GENERIC_STATS_API(port)

    vs_clear_port_all_stats,

    VS_GENERIC_QUAD_API(port_pool)
    VS_GENERIC_STATS_API(port_pool)
};
