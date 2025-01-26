
/* DO NOT MODIFY, FILE AUTO GENERATED */

extern "C" {
#include "otai.h"
}
#include "meta/OtaiInterface.h"
#include "ClientServerOtai.h"
#include "swss/logger.h"
#include <memory>

static std::shared_ptr<otairedis::OtaiInterface> stub = std::make_shared<otairedis::ClientServerOtai>();


/* ==== API STRUCTS === */


/* OTAI APIS for LINECARD */

static otai_status_t stub_create_linecard(
        _Out_ otai_object_id_t *linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_linecard(
        _In_ otai_object_id_t linecard_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id);
}

static otai_status_t stub_set_linecard_attribute(
        _In_ otai_object_id_t linecard_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id, attr);
}

static otai_status_t stub_get_linecard_attribute(
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id, attr_count, attr_list);
}

static otai_status_t stub_get_linecard_stats(
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_linecard_stats_ext(
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_linecard_stats(
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_LINECARD),linecard_id, number_of_counters, counter_ids);
}

static otai_linecard_api_t stub_linecard = {
    .create_linecard = stub_create_linecard,
    .remove_linecard = stub_remove_linecard,
    .set_linecard_attribute = stub_set_linecard_attribute,
    .get_linecard_attribute = stub_get_linecard_attribute,
    .get_linecard_stats = stub_get_linecard_stats,
    .get_linecard_stats_ext = stub_get_linecard_stats_ext,
    .clear_linecard_stats = stub_clear_linecard_stats,
};


/* OTAI APIS for PORT */

static otai_status_t stub_create_port(
        _Out_ otai_object_id_t *port_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_port(
        _In_ otai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id);
}

static otai_status_t stub_set_port_attribute(
        _In_ otai_object_id_t port_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id, attr);
}

static otai_status_t stub_get_port_attribute(
        _In_ otai_object_id_t port_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id, attr_count, attr_list);
}

static otai_status_t stub_get_port_stats(
        _In_ otai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_port_stats_ext(
        _In_ otai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_port_stats(
        _In_ otai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_PORT),port_id, number_of_counters, counter_ids);
}

static otai_port_api_t stub_port = {
    .create_port = stub_create_port,
    .remove_port = stub_remove_port,
    .set_port_attribute = stub_set_port_attribute,
    .get_port_attribute = stub_get_port_attribute,
    .get_port_stats = stub_get_port_stats,
    .get_port_stats_ext = stub_get_port_stats_ext,
    .clear_port_stats = stub_clear_port_stats,
};


/* OTAI APIS for TRANSCEIVER */

static otai_status_t stub_create_transceiver(
        _Out_ otai_object_id_t *transceiver_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_transceiver(
        _In_ otai_object_id_t transceiver_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id);
}

static otai_status_t stub_set_transceiver_attribute(
        _In_ otai_object_id_t transceiver_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id, attr);
}

static otai_status_t stub_get_transceiver_attribute(
        _In_ otai_object_id_t transceiver_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id, attr_count, attr_list);
}

static otai_status_t stub_get_transceiver_stats(
        _In_ otai_object_id_t transceiver_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_transceiver_stats_ext(
        _In_ otai_object_id_t transceiver_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_transceiver_stats(
        _In_ otai_object_id_t transceiver_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_TRANSCEIVER),transceiver_id, number_of_counters, counter_ids);
}

static otai_transceiver_api_t stub_transceiver = {
    .create_transceiver = stub_create_transceiver,
    .remove_transceiver = stub_remove_transceiver,
    .set_transceiver_attribute = stub_set_transceiver_attribute,
    .get_transceiver_attribute = stub_get_transceiver_attribute,
    .get_transceiver_stats = stub_get_transceiver_stats,
    .get_transceiver_stats_ext = stub_get_transceiver_stats_ext,
    .clear_transceiver_stats = stub_clear_transceiver_stats,
};


/* OTAI APIS for LOGICALCHANNEL */

static otai_status_t stub_create_logicalchannel(
        _Out_ otai_object_id_t *logicalchannel_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_logicalchannel(
        _In_ otai_object_id_t logicalchannel_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id);
}

static otai_status_t stub_set_logicalchannel_attribute(
        _In_ otai_object_id_t logicalchannel_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id, attr);
}

static otai_status_t stub_get_logicalchannel_attribute(
        _In_ otai_object_id_t logicalchannel_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id, attr_count, attr_list);
}

static otai_status_t stub_get_logicalchannel_stats(
        _In_ otai_object_id_t logicalchannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_logicalchannel_stats_ext(
        _In_ otai_object_id_t logicalchannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_logicalchannel_stats(
        _In_ otai_object_id_t logicalchannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_LOGICALCHANNEL),logicalchannel_id, number_of_counters, counter_ids);
}

static otai_logicalchannel_api_t stub_logicalchannel = {
    .create_logicalchannel = stub_create_logicalchannel,
    .remove_logicalchannel = stub_remove_logicalchannel,
    .set_logicalchannel_attribute = stub_set_logicalchannel_attribute,
    .get_logicalchannel_attribute = stub_get_logicalchannel_attribute,
    .get_logicalchannel_stats = stub_get_logicalchannel_stats,
    .get_logicalchannel_stats_ext = stub_get_logicalchannel_stats_ext,
    .clear_logicalchannel_stats = stub_clear_logicalchannel_stats,
};


/* OTAI APIS for OTN */

static otai_status_t stub_create_otn(
        _Out_ otai_object_id_t *otn_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_otn(
        _In_ otai_object_id_t otn_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id);
}

static otai_status_t stub_set_otn_attribute(
        _In_ otai_object_id_t otn_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id, attr);
}

static otai_status_t stub_get_otn_attribute(
        _In_ otai_object_id_t otn_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id, attr_count, attr_list);
}

static otai_status_t stub_get_otn_stats(
        _In_ otai_object_id_t otn_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_otn_stats_ext(
        _In_ otai_object_id_t otn_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_otn_stats(
        _In_ otai_object_id_t otn_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OTN),otn_id, number_of_counters, counter_ids);
}

static otai_otn_api_t stub_otn = {
    .create_otn = stub_create_otn,
    .remove_otn = stub_remove_otn,
    .set_otn_attribute = stub_set_otn_attribute,
    .get_otn_attribute = stub_get_otn_attribute,
    .get_otn_stats = stub_get_otn_stats,
    .get_otn_stats_ext = stub_get_otn_stats_ext,
    .clear_otn_stats = stub_clear_otn_stats,
};


/* OTAI APIS for ETHERNET */

static otai_status_t stub_create_ethernet(
        _Out_ otai_object_id_t *ethernet_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_ethernet(
        _In_ otai_object_id_t ethernet_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id);
}

static otai_status_t stub_set_ethernet_attribute(
        _In_ otai_object_id_t ethernet_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id, attr);
}

static otai_status_t stub_get_ethernet_attribute(
        _In_ otai_object_id_t ethernet_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id, attr_count, attr_list);
}

static otai_status_t stub_get_ethernet_stats(
        _In_ otai_object_id_t ethernet_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_ethernet_stats_ext(
        _In_ otai_object_id_t ethernet_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_ethernet_stats(
        _In_ otai_object_id_t ethernet_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_ETHERNET),ethernet_id, number_of_counters, counter_ids);
}

static otai_ethernet_api_t stub_ethernet = {
    .create_ethernet = stub_create_ethernet,
    .remove_ethernet = stub_remove_ethernet,
    .set_ethernet_attribute = stub_set_ethernet_attribute,
    .get_ethernet_attribute = stub_get_ethernet_attribute,
    .get_ethernet_stats = stub_get_ethernet_stats,
    .get_ethernet_stats_ext = stub_get_ethernet_stats_ext,
    .clear_ethernet_stats = stub_clear_ethernet_stats,
};


/* OTAI APIS for PHYSICALCHANNEL */

static otai_status_t stub_create_physicalchannel(
        _Out_ otai_object_id_t *physicalchannel_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_physicalchannel(
        _In_ otai_object_id_t physicalchannel_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id);
}

static otai_status_t stub_set_physicalchannel_attribute(
        _In_ otai_object_id_t physicalchannel_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id, attr);
}

static otai_status_t stub_get_physicalchannel_attribute(
        _In_ otai_object_id_t physicalchannel_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id, attr_count, attr_list);
}

static otai_status_t stub_get_physicalchannel_stats(
        _In_ otai_object_id_t physicalchannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_physicalchannel_stats_ext(
        _In_ otai_object_id_t physicalchannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_physicalchannel_stats(
        _In_ otai_object_id_t physicalchannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_PHYSICALCHANNEL),physicalchannel_id, number_of_counters, counter_ids);
}

static otai_physicalchannel_api_t stub_physicalchannel = {
    .create_physicalchannel = stub_create_physicalchannel,
    .remove_physicalchannel = stub_remove_physicalchannel,
    .set_physicalchannel_attribute = stub_set_physicalchannel_attribute,
    .get_physicalchannel_attribute = stub_get_physicalchannel_attribute,
    .get_physicalchannel_stats = stub_get_physicalchannel_stats,
    .get_physicalchannel_stats_ext = stub_get_physicalchannel_stats_ext,
    .clear_physicalchannel_stats = stub_clear_physicalchannel_stats,
};


/* OTAI APIS for OCH */

static otai_status_t stub_create_och(
        _Out_ otai_object_id_t *och_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_och(
        _In_ otai_object_id_t och_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id);
}

static otai_status_t stub_set_och_attribute(
        _In_ otai_object_id_t och_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id, attr);
}

static otai_status_t stub_get_och_attribute(
        _In_ otai_object_id_t och_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id, attr_count, attr_list);
}

static otai_status_t stub_get_och_stats(
        _In_ otai_object_id_t och_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_och_stats_ext(
        _In_ otai_object_id_t och_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_och_stats(
        _In_ otai_object_id_t och_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OCH),och_id, number_of_counters, counter_ids);
}

static otai_och_api_t stub_och = {
    .create_och = stub_create_och,
    .remove_och = stub_remove_och,
    .set_och_attribute = stub_set_och_attribute,
    .get_och_attribute = stub_get_och_attribute,
    .get_och_stats = stub_get_och_stats,
    .get_och_stats_ext = stub_get_och_stats_ext,
    .clear_och_stats = stub_clear_och_stats,
};


/* OTAI APIS for LLDP */

static otai_status_t stub_create_lldp(
        _Out_ otai_object_id_t *lldp_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_lldp(
        _In_ otai_object_id_t lldp_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id);
}

static otai_status_t stub_set_lldp_attribute(
        _In_ otai_object_id_t lldp_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id, attr);
}

static otai_status_t stub_get_lldp_attribute(
        _In_ otai_object_id_t lldp_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id, attr_count, attr_list);
}

static otai_status_t stub_get_lldp_stats(
        _In_ otai_object_id_t lldp_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_lldp_stats_ext(
        _In_ otai_object_id_t lldp_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_lldp_stats(
        _In_ otai_object_id_t lldp_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_LLDP),lldp_id, number_of_counters, counter_ids);
}

static otai_lldp_api_t stub_lldp = {
    .create_lldp = stub_create_lldp,
    .remove_lldp = stub_remove_lldp,
    .set_lldp_attribute = stub_set_lldp_attribute,
    .get_lldp_attribute = stub_get_lldp_attribute,
    .get_lldp_stats = stub_get_lldp_stats,
    .get_lldp_stats_ext = stub_get_lldp_stats_ext,
    .clear_lldp_stats = stub_clear_lldp_stats,
};


/* OTAI APIS for ASSIGNMENT */

static otai_status_t stub_create_assignment(
        _Out_ otai_object_id_t *assignment_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_assignment(
        _In_ otai_object_id_t assignment_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id);
}

static otai_status_t stub_set_assignment_attribute(
        _In_ otai_object_id_t assignment_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id, attr);
}

static otai_status_t stub_get_assignment_attribute(
        _In_ otai_object_id_t assignment_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id, attr_count, attr_list);
}

static otai_status_t stub_get_assignment_stats(
        _In_ otai_object_id_t assignment_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_assignment_stats_ext(
        _In_ otai_object_id_t assignment_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_assignment_stats(
        _In_ otai_object_id_t assignment_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_ASSIGNMENT),assignment_id, number_of_counters, counter_ids);
}

static otai_assignment_api_t stub_assignment = {
    .create_assignment = stub_create_assignment,
    .remove_assignment = stub_remove_assignment,
    .set_assignment_attribute = stub_set_assignment_attribute,
    .get_assignment_attribute = stub_get_assignment_attribute,
    .get_assignment_stats = stub_get_assignment_stats,
    .get_assignment_stats_ext = stub_get_assignment_stats_ext,
    .clear_assignment_stats = stub_clear_assignment_stats,
};


/* OTAI APIS for INTERFACE */

static otai_status_t stub_create_interface(
        _Out_ otai_object_id_t *interface_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_interface(
        _In_ otai_object_id_t interface_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id);
}

static otai_status_t stub_set_interface_attribute(
        _In_ otai_object_id_t interface_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id, attr);
}

static otai_status_t stub_get_interface_attribute(
        _In_ otai_object_id_t interface_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id, attr_count, attr_list);
}

static otai_status_t stub_get_interface_stats(
        _In_ otai_object_id_t interface_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_interface_stats_ext(
        _In_ otai_object_id_t interface_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_interface_stats(
        _In_ otai_object_id_t interface_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_INTERFACE),interface_id, number_of_counters, counter_ids);
}

static otai_interface_api_t stub_interface = {
    .create_interface = stub_create_interface,
    .remove_interface = stub_remove_interface,
    .set_interface_attribute = stub_set_interface_attribute,
    .get_interface_attribute = stub_get_interface_attribute,
    .get_interface_stats = stub_get_interface_stats,
    .get_interface_stats_ext = stub_get_interface_stats_ext,
    .clear_interface_stats = stub_clear_interface_stats,
};


/* OTAI APIS for OA */

static otai_status_t stub_create_oa(
        _Out_ otai_object_id_t *oa_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_oa(
        _In_ otai_object_id_t oa_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id);
}

static otai_status_t stub_set_oa_attribute(
        _In_ otai_object_id_t oa_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id, attr);
}

static otai_status_t stub_get_oa_attribute(
        _In_ otai_object_id_t oa_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id, attr_count, attr_list);
}

static otai_status_t stub_get_oa_stats(
        _In_ otai_object_id_t oa_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_oa_stats_ext(
        _In_ otai_object_id_t oa_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_oa_stats(
        _In_ otai_object_id_t oa_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OA),oa_id, number_of_counters, counter_ids);
}

static otai_oa_api_t stub_oa = {
    .create_oa = stub_create_oa,
    .remove_oa = stub_remove_oa,
    .set_oa_attribute = stub_set_oa_attribute,
    .get_oa_attribute = stub_get_oa_attribute,
    .get_oa_stats = stub_get_oa_stats,
    .get_oa_stats_ext = stub_get_oa_stats_ext,
    .clear_oa_stats = stub_clear_oa_stats,
};


/* OTAI APIS for OSC */

static otai_status_t stub_create_osc(
        _Out_ otai_object_id_t *osc_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_osc(
        _In_ otai_object_id_t osc_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id);
}

static otai_status_t stub_set_osc_attribute(
        _In_ otai_object_id_t osc_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id, attr);
}

static otai_status_t stub_get_osc_attribute(
        _In_ otai_object_id_t osc_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id, attr_count, attr_list);
}

static otai_status_t stub_get_osc_stats(
        _In_ otai_object_id_t osc_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_osc_stats_ext(
        _In_ otai_object_id_t osc_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_osc_stats(
        _In_ otai_object_id_t osc_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OSC),osc_id, number_of_counters, counter_ids);
}

static otai_osc_api_t stub_osc = {
    .create_osc = stub_create_osc,
    .remove_osc = stub_remove_osc,
    .set_osc_attribute = stub_set_osc_attribute,
    .get_osc_attribute = stub_get_osc_attribute,
    .get_osc_stats = stub_get_osc_stats,
    .get_osc_stats_ext = stub_get_osc_stats_ext,
    .clear_osc_stats = stub_clear_osc_stats,
};


/* OTAI APIS for APS */

static otai_status_t stub_create_aps(
        _Out_ otai_object_id_t *aps_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_aps(
        _In_ otai_object_id_t aps_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id);
}

static otai_status_t stub_set_aps_attribute(
        _In_ otai_object_id_t aps_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id, attr);
}

static otai_status_t stub_get_aps_attribute(
        _In_ otai_object_id_t aps_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id, attr_count, attr_list);
}

static otai_status_t stub_get_aps_stats(
        _In_ otai_object_id_t aps_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_aps_stats_ext(
        _In_ otai_object_id_t aps_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_aps_stats(
        _In_ otai_object_id_t aps_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_APS),aps_id, number_of_counters, counter_ids);
}

static otai_aps_api_t stub_aps = {
    .create_aps = stub_create_aps,
    .remove_aps = stub_remove_aps,
    .set_aps_attribute = stub_set_aps_attribute,
    .get_aps_attribute = stub_get_aps_attribute,
    .get_aps_stats = stub_get_aps_stats,
    .get_aps_stats_ext = stub_get_aps_stats_ext,
    .clear_aps_stats = stub_clear_aps_stats,
};


/* OTAI APIS for APSPORT */

static otai_status_t stub_create_apsport(
        _Out_ otai_object_id_t *apsport_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_apsport(
        _In_ otai_object_id_t apsport_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id);
}

static otai_status_t stub_set_apsport_attribute(
        _In_ otai_object_id_t apsport_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id, attr);
}

static otai_status_t stub_get_apsport_attribute(
        _In_ otai_object_id_t apsport_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id, attr_count, attr_list);
}

static otai_status_t stub_get_apsport_stats(
        _In_ otai_object_id_t apsport_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_apsport_stats_ext(
        _In_ otai_object_id_t apsport_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_apsport_stats(
        _In_ otai_object_id_t apsport_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_APSPORT),apsport_id, number_of_counters, counter_ids);
}

static otai_apsport_api_t stub_apsport = {
    .create_apsport = stub_create_apsport,
    .remove_apsport = stub_remove_apsport,
    .set_apsport_attribute = stub_set_apsport_attribute,
    .get_apsport_attribute = stub_get_apsport_attribute,
    .get_apsport_stats = stub_get_apsport_stats,
    .get_apsport_stats_ext = stub_get_apsport_stats_ext,
    .clear_apsport_stats = stub_clear_apsport_stats,
};


/* OTAI APIS for ATTENUATOR */

static otai_status_t stub_create_attenuator(
        _Out_ otai_object_id_t *attenuator_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_attenuator(
        _In_ otai_object_id_t attenuator_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id);
}

static otai_status_t stub_set_attenuator_attribute(
        _In_ otai_object_id_t attenuator_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id, attr);
}

static otai_status_t stub_get_attenuator_attribute(
        _In_ otai_object_id_t attenuator_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id, attr_count, attr_list);
}

static otai_status_t stub_get_attenuator_stats(
        _In_ otai_object_id_t attenuator_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_attenuator_stats_ext(
        _In_ otai_object_id_t attenuator_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_attenuator_stats(
        _In_ otai_object_id_t attenuator_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_ATTENUATOR),attenuator_id, number_of_counters, counter_ids);
}

static otai_attenuator_api_t stub_attenuator = {
    .create_attenuator = stub_create_attenuator,
    .remove_attenuator = stub_remove_attenuator,
    .set_attenuator_attribute = stub_set_attenuator_attribute,
    .get_attenuator_attribute = stub_get_attenuator_attribute,
    .get_attenuator_stats = stub_get_attenuator_stats,
    .get_attenuator_stats_ext = stub_get_attenuator_stats_ext,
    .clear_attenuator_stats = stub_clear_attenuator_stats,
};


/* OTAI APIS for WSS */

static otai_status_t stub_create_wss(
        _Out_ otai_object_id_t *wss_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_wss(
        _In_ otai_object_id_t wss_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id);
}

static otai_status_t stub_set_wss_attribute(
        _In_ otai_object_id_t wss_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id, attr);
}

static otai_status_t stub_get_wss_attribute(
        _In_ otai_object_id_t wss_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id, attr_count, attr_list);
}

static otai_status_t stub_get_wss_stats(
        _In_ otai_object_id_t wss_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_wss_stats_ext(
        _In_ otai_object_id_t wss_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_wss_stats(
        _In_ otai_object_id_t wss_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_WSS),wss_id, number_of_counters, counter_ids);
}

static otai_wss_api_t stub_wss = {
    .create_wss = stub_create_wss,
    .remove_wss = stub_remove_wss,
    .set_wss_attribute = stub_set_wss_attribute,
    .get_wss_attribute = stub_get_wss_attribute,
    .get_wss_stats = stub_get_wss_stats,
    .get_wss_stats_ext = stub_get_wss_stats_ext,
    .clear_wss_stats = stub_clear_wss_stats,
};


/* OTAI APIS for MEDIACHANNEL */

static otai_status_t stub_create_mediachannel(
        _Out_ otai_object_id_t *mediachannel_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_mediachannel(
        _In_ otai_object_id_t mediachannel_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id);
}

static otai_status_t stub_set_mediachannel_attribute(
        _In_ otai_object_id_t mediachannel_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id, attr);
}

static otai_status_t stub_get_mediachannel_attribute(
        _In_ otai_object_id_t mediachannel_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id, attr_count, attr_list);
}

static otai_status_t stub_get_mediachannel_stats(
        _In_ otai_object_id_t mediachannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_mediachannel_stats_ext(
        _In_ otai_object_id_t mediachannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_mediachannel_stats(
        _In_ otai_object_id_t mediachannel_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_MEDIACHANNEL),mediachannel_id, number_of_counters, counter_ids);
}

static otai_mediachannel_api_t stub_mediachannel = {
    .create_mediachannel = stub_create_mediachannel,
    .remove_mediachannel = stub_remove_mediachannel,
    .set_mediachannel_attribute = stub_set_mediachannel_attribute,
    .get_mediachannel_attribute = stub_get_mediachannel_attribute,
    .get_mediachannel_stats = stub_get_mediachannel_stats,
    .get_mediachannel_stats_ext = stub_get_mediachannel_stats_ext,
    .clear_mediachannel_stats = stub_clear_mediachannel_stats,
};


/* OTAI APIS for OCM */

static otai_status_t stub_create_ocm(
        _Out_ otai_object_id_t *ocm_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_ocm(
        _In_ otai_object_id_t ocm_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id);
}

static otai_status_t stub_set_ocm_attribute(
        _In_ otai_object_id_t ocm_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id, attr);
}

static otai_status_t stub_get_ocm_attribute(
        _In_ otai_object_id_t ocm_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id, attr_count, attr_list);
}

static otai_status_t stub_get_ocm_stats(
        _In_ otai_object_id_t ocm_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_ocm_stats_ext(
        _In_ otai_object_id_t ocm_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_ocm_stats(
        _In_ otai_object_id_t ocm_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OCM),ocm_id, number_of_counters, counter_ids);
}

static otai_ocm_api_t stub_ocm = {
    .create_ocm = stub_create_ocm,
    .remove_ocm = stub_remove_ocm,
    .set_ocm_attribute = stub_set_ocm_attribute,
    .get_ocm_attribute = stub_get_ocm_attribute,
    .get_ocm_stats = stub_get_ocm_stats,
    .get_ocm_stats_ext = stub_get_ocm_stats_ext,
    .clear_ocm_stats = stub_clear_ocm_stats,
};


/* OTAI APIS for OTDR */

static otai_status_t stub_create_otdr(
        _Out_ otai_object_id_t *otdr_id,
        _In_ otai_object_id_t linecard_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->create((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id, linecard_id, attr_count, attr_list);
}

static otai_status_t stub_remove_otdr(
        _In_ otai_object_id_t otdr_id)
{
    SWSS_LOG_ENTER();

    return stub->remove((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id);
}

static otai_status_t stub_set_otdr_attribute(
        _In_ otai_object_id_t otdr_id,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return stub->set((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id, attr);
}

static otai_status_t stub_get_otdr_attribute(
        _In_ otai_object_id_t otdr_id,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub->get((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id, attr_count, attr_list);
}

static otai_status_t stub_get_otdr_stats(
        _In_ otai_object_id_t otdr_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id, number_of_counters, counter_ids, counters);
}

static otai_status_t stub_get_otdr_stats_ext(
        _In_ otai_object_id_t otdr_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ otai_stat_value_t *counters)
{
    SWSS_LOG_ENTER();

    return stub->getStatsExt((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id, number_of_counters, counter_ids, mode, counters);
}

static otai_status_t stub_clear_otdr_stats(
        _In_ otai_object_id_t otdr_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return stub->clearStats((otai_object_type_t)(OTAI_OBJECT_TYPE_OTDR),otdr_id, number_of_counters, counter_ids);
}

static otai_otdr_api_t stub_otdr = {
    .create_otdr = stub_create_otdr,
    .remove_otdr = stub_remove_otdr,
    .set_otdr_attribute = stub_set_otdr_attribute,
    .get_otdr_attribute = stub_get_otdr_attribute,
    .get_otdr_stats = stub_get_otdr_stats,
    .get_otdr_stats_ext = stub_get_otdr_stats_ext,
    .clear_otdr_stats = stub_clear_otdr_stats,
};


/* ==== API QUERY === */

otai_status_t otai_api_query(
        _In_ otai_api_t api,
        _Out_ void** api_method_table)
{
    SWSS_LOG_ENTER();

    if (api_method_table == NULL)
    {
        SWSS_LOG_ERROR("NULL method table passed to OTAI API initialize");

        return OTAI_STATUS_INVALID_PARAMETER;
    }

    if (api == OTAI_API_UNSPECIFIED)
    {
        SWSS_LOG_ERROR("api ID is unspecified api");

        return OTAI_STATUS_INVALID_PARAMETER;
    }

    switch((int)api)
    {
        case OTAI_API_LINECARD:
            *api_method_table = (void**)&stub_linecard;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_PORT:
            *api_method_table = (void**)&stub_port;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_TRANSCEIVER:
            *api_method_table = (void**)&stub_transceiver;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_LOGICALCHANNEL:
            *api_method_table = (void**)&stub_logicalchannel;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_OTN:
            *api_method_table = (void**)&stub_otn;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_ETHERNET:
            *api_method_table = (void**)&stub_ethernet;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_PHYSICALCHANNEL:
            *api_method_table = (void**)&stub_physicalchannel;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_OCH:
            *api_method_table = (void**)&stub_och;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_LLDP:
            *api_method_table = (void**)&stub_lldp;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_ASSIGNMENT:
            *api_method_table = (void**)&stub_assignment;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_INTERFACE:
            *api_method_table = (void**)&stub_interface;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_OA:
            *api_method_table = (void**)&stub_oa;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_OSC:
            *api_method_table = (void**)&stub_osc;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_APS:
            *api_method_table = (void**)&stub_aps;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_APSPORT:
            *api_method_table = (void**)&stub_apsport;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_ATTENUATOR:
            *api_method_table = (void**)&stub_attenuator;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_WSS:
            *api_method_table = (void**)&stub_wss;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_MEDIACHANNEL:
            *api_method_table = (void**)&stub_mediachannel;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_OCM:
            *api_method_table = (void**)&stub_ocm;
            return OTAI_STATUS_SUCCESS;
        case OTAI_API_OTDR:
            *api_method_table = (void**)&stub_otdr;
            return OTAI_STATUS_SUCCESS;
        default:
            break;
    }

    SWSS_LOG_ERROR("Invalid API type %d", api);

    return OTAI_STATUS_INVALID_PARAMETER;
}


/* ==== GLOBAL APIS === */

otai_object_id_t otai_linecard_id_query(
        _In_ otai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    return stub->linecardIdQuery(object_id);
}

otai_object_type_t otai_object_type_query(
        _In_ otai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    return stub->objectTypeQuery(object_id);
}

otai_status_t otai_api_initialize(
        _In_ uint64_t flags,
        _In_ const otai_service_method_table_t *services)
{
    SWSS_LOG_ENTER();

    return stub->apiInitialize(flags, services);
}

otai_status_t otai_api_uninitialize(void)
{
    SWSS_LOG_ENTER();

    return stub->apiUninitialize();
}

otai_status_t otai_dbg_generate_dump(
        _In_ const char *dump_file_name)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("FIXME, no implementation for dbgGenerateDump!");
    return OTAI_STATUS_NOT_IMPLEMENTED;
}
otai_status_t otai_link_check(
        _Out_ bool *up)
{
    SWSS_LOG_ENTER();

    return stub->linkCheck(up);
}

otai_status_t otai_log_set(
        _In_ otai_api_t api,
        _In_ otai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return stub->logSet(api, log_level);
}

