/* Exercise private reply handlers without opening a live VPP API socket. */
#include "../../vslib/vpp/vppxlate/SaiVppXlate.c"

#define TEST_CORRELATED_HANDLER(base, reply_type, handler)                 \
do {                                                                      \
    vpp_correlated_request_t expired_request;                             \
    vpp_correlated_request_t current_request;                             \
    uint32_t expired_context = begin_correlated_request(                  \
            &expired_request, NULL);                                      \
    end_correlated_request(expired_context);                              \
    uint32_t live_context = begin_correlated_request(                     \
            &current_request, NULL);                                      \
    reply_type correlated_reply = {0};                                    \
    correlated_reply.context = expired_context;                           \
    correlated_reply.retval = htonl((uint32_t)-5);                        \
    handler(&correlated_reply);                                           \
    if (current_request.result_ready ||                                   \
        get_index_ptr(live_context) != (uintptr_t)&current_request)        \
        return (base);                                                    \
    correlated_reply.context = VPP_INVALID_CTX_INDEX;                     \
    handler(&correlated_reply);                                           \
    if (current_request.result_ready)                                     \
        return (base) + 1;                                                \
    vat_main.result_ready = 0;                                            \
    set_reply_status(-19);                                                \
    if (current_request.result_ready || !vat_main.result_ready)           \
        return (base) + 2;                                                \
    correlated_reply.context = live_context;                              \
    correlated_reply.retval = htonl((uint32_t)-7);                        \
    handler(&correlated_reply);                                           \
    if (!current_request.result_ready || current_request.retval != -7 ||  \
        get_index_ptr(live_context))                                      \
        return (base) + 3;                                                \
    correlated_reply.retval = 0;                                         \
    handler(&correlated_reply);                                           \
    if (current_request.retval != -7)                                     \
        return (base) + 4;                                                \
} while (0)

int main(void)
{
    vpp_sonic_ext_trim_counters_t expired = {0};
    vpp_sonic_ext_trim_counters_t current = {0};
    uint32_t old_context = store_ptr(&expired);
    release_index(old_context);
    uint32_t current_context = store_ptr(&current);

    if (old_context == current_context ||
        (old_context & VPP_CTX_INDEX_MASK) != (current_context & VPP_CTX_INDEX_MASK))
        return 1;

    vl_api_sonic_ext_trim_counters_get_reply_t reply = {0};
    reply.context = old_context;
    reply.retval = htonl((uint32_t)-9);
    vat_main.retval = 123;
    vat_main.result_ready = 0;
    vl_api_sonic_ext_trim_counters_get_reply_t_handler(&reply);

    if (vat_main.result_ready || vat_main.retval != 123 ||
        current.trim_sent || current.trim_drop || current.trim_admit_fail ||
        get_index_ptr(current_context) != (uintptr_t)&current)
        return 2;

    reply.context = current_context;
    reply.retval = 0;
    reply.trim_sent = htobe64(11);
    reply.trim_drop = htobe64(12);
    reply.trim_admit_fail = htobe64(13);
    vl_api_sonic_ext_trim_counters_get_reply_t_handler(&reply);

    if (!vat_main.result_ready || vat_main.retval ||
        current.trim_sent != 11 || current.trim_drop != 12 ||
        current.trim_admit_fail != 13 || get_index_ptr(current_context))
        return 3;

    vat_main.result_ready = 0;
    reply.context = VPP_INVALID_CTX_INDEX;
    vl_api_sonic_ext_trim_counters_get_reply_t_handler(&reply);
    if (vat_main.result_ready)
        return 4;

    int request = 0;
    old_context = store_ptr(&request);
    release_index(old_context);
    current_context = store_ptr(&request);
    vl_api_sonic_ext_trim_global_set_reply_t global = {0};
    vl_api_sonic_ext_trim_dscp_map_set_reply_t map = {0};
    vl_api_sonic_ext_trim_queue_set_reply_t queue = {0};
    global.context = map.context = queue.context = old_context;
    vat_main.retval = 123;
    vl_api_sonic_ext_trim_global_set_reply_t_handler(&global);
    vl_api_sonic_ext_trim_dscp_map_set_reply_t_handler(&map);
    vl_api_sonic_ext_trim_queue_set_reply_t_handler(&queue);
    if (vat_main.result_ready || vat_main.retval != 123 ||
        get_index_ptr(current_context) != (uintptr_t)&request)
        return 5;

    global.context = current_context;
    global.retval = htonl((uint32_t)-17);
    vl_api_sonic_ext_trim_global_set_reply_t_handler(&global);
    if (!vat_main.result_ready || vat_main.retval != -17 ||
        get_index_ptr(current_context))
        return 6;

    vat_main.result_ready = 0;
    map.context = store_ptr(&request);
    vl_api_sonic_ext_trim_dscp_map_set_reply_t_handler(&map);
    if (!vat_main.result_ready || vat_main.retval || get_index_ptr(map.context))
        return 7;

    vat_main.result_ready = 0;
    queue.context = store_ptr(&request);
    queue.retval = htonl((uint32_t)-5);
    vl_api_sonic_ext_trim_queue_set_reply_t_handler(&queue);
    if (!vat_main.result_ready || vat_main.retval != -5 || get_index_ptr(queue.context))
        return 8;

    vat_main.result_ready = 0;
    vl_api_sonic_ext_trim_queue_set_reply_t_handler(&queue);
    global.context = map.context = queue.context = VPP_INVALID_CTX_INDEX;
    vl_api_sonic_ext_trim_global_set_reply_t_handler(&global);
    vl_api_sonic_ext_trim_dscp_map_set_reply_t_handler(&map);
    vl_api_sonic_ext_trim_queue_set_reply_t_handler(&queue);
    if (vat_main.result_ready)
        return 9;

    /* Route replies must also be request-correlated. A released context
     * models a late reply from a timed-out route request. The NULL stats path
     * still gets a live context, and unrelated generic completion cannot
     * satisfy the dedicated route wait. */
    uint32_t route_stats = UINT32_MAX;
    old_context = store_ptr(&route_stats);
    release_index(old_context);
    current_context = begin_route_request(NULL, &route_stats);
    if (current_context == VPP_INVALID_CTX_INDEX ||
        get_index_ptr(current_context) != (uintptr_t)&route_stats)
        return 10;

    vl_api_ip_route_add_del_reply_t route = {0};
    route.context = old_context;
    route.retval = 0;
    route.stats_index = htonl(41);
    vat_main.retval = 123;
    vat_main.result_ready = 0;
    vl_api_ip_route_add_del_reply_t_handler(&route);
    if (route_result_ready || vat_main.result_ready || vat_main.retval != 123 ||
        route_stats != UINT32_MAX ||
        get_index_ptr(current_context) != (uintptr_t)&route_stats)
        return 11;

    set_reply_status(-17);
    if (route_result_ready || !vat_main.result_ready || vat_main.retval != -17)
        return 12;

    route.context = 0;
    vl_api_ip_route_add_del_reply_t_handler(&route);
    if (route_result_ready || route_stats != UINT32_MAX)
        return 13;

    route.context = current_context;
    route.stats_index = htonl(42);
    vl_api_ip_route_add_del_reply_t_handler(&route);
    if (!route_result_ready || route_retval || route_stats != 42 ||
        get_index_ptr(current_context))
        return 14;
    end_route_request(current_context);
    if (route_expected_context != VPP_INVALID_CTX_INDEX)
        return 15;

    /* Multicast routes share the isolated route completion state. A late
     * reply from a released request cannot complete the current wait, and an
     * unrelated generic reply cannot make WR_ROUTE report success. */
    uint32_t mroute_marker = UINT32_MAX;
    old_context = store_ptr(&mroute_marker);
    release_index(old_context);
    current_context = begin_route_request(NULL, &mroute_marker);
    if (current_context == VPP_INVALID_CTX_INDEX)
        return 16;

    vl_api_ip_mroute_add_del_reply_t mroute = {0};
    mroute.context = old_context;
    mroute.retval = 0;
    vat_main.result_ready = 0;
    vl_api_ip_mroute_add_del_reply_t_handler(&mroute);
    if (route_result_ready || vat_main.result_ready ||
        get_index_ptr(current_context) != (uintptr_t)&mroute_marker)
        return 17;

    set_reply_status(-19);
    if (route_result_ready || !vat_main.result_ready || vat_main.retval != -19)
        return 18;

    mroute.context = current_context;
    mroute.retval = htonl((uint32_t)-7);
    vl_api_ip_mroute_add_del_reply_t_handler(&mroute);
    if (!route_result_ready || route_retval != -7 || get_index_ptr(current_context))
        return 19;
    end_route_request(current_context);
    if (route_expected_context != VPP_INVALID_CTX_INDEX)
        return 20;

    /*
     * A MAC age request times out, and its reply, a success, arrives while
     * the next request is pending: it completes nothing.
     */
    int mac_age_request = 0;
    vl_api_bridge_domain_set_mac_age_reply_t mac_age = {0};

    old_context = store_ptr(&mac_age_request);
    if (finish_tagged_request(old_context, -99) != -99 || get_index_ptr(old_context))
        return 21;

    current_context = store_ptr(&mac_age_request);
    mac_age.context = old_context;
    vat_main.retval = 123;
    vat_main.result_ready = 0;
    vl_api_bridge_domain_set_mac_age_reply_t_handler(&mac_age);
    if (vat_main.result_ready || vat_main.retval != 123 ||
        get_index_ptr(current_context) != (uintptr_t)&mac_age_request)
        return 22;

    /* WR() ended on another request's reply: the pending request fails */
    if (finish_tagged_request(current_context, 0) != -EAGAIN || get_index_ptr(current_context))
        return 23;

    /* The request's own reply completes it, with VPP's result */
    current_context = store_ptr(&mac_age_request);
    mac_age.context = current_context;
    mac_age.retval = htonl((uint32_t)-3);
    vl_api_bridge_domain_set_mac_age_reply_t_handler(&mac_age);
    if (!vat_main.result_ready || vat_main.retval != -3 || get_index_ptr(current_context) ||
        finish_tagged_request(current_context, vat_main.retval) != -3)
        return 24;

    /* Every newly transactional mutation waits on its own request state.
     * Late, zero-context and unrelated replies must not complete it. */
    TEST_CORRELATED_HANDLER(30, vl_api_sw_interface_set_flags_reply_t,
            vl_api_sw_interface_set_flags_reply_t_handler);
    TEST_CORRELATED_HANDLER(40,
            vl_api_sw_interface_ip6_enable_disable_reply_t,
            vl_api_sw_interface_ip6_enable_disable_reply_t_handler);
    TEST_CORRELATED_HANDLER(50,
            vl_api_classify_add_del_session_reply_t,
            vl_api_classify_add_del_session_reply_t_handler);
    TEST_CORRELATED_HANDLER(60,
            vl_api_classify_set_interface_l2_tables_reply_t,
            vl_api_classify_set_interface_l2_tables_reply_t_handler);
    TEST_CORRELATED_HANDLER(70, vl_api_l2_flags_set_reply_t,
            vl_api_l2_flags_set_reply_t_handler);
    TEST_CORRELATED_HANDLER(75,
            vl_api_sonic_ext_rif_ip6_enable_disable_reply_t,
            vl_api_sonic_ext_rif_ip6_enable_disable_reply_t_handler);

    uint32_t expired_table = 99;
    uint32_t current_table = 99;
    vpp_correlated_request_t expired_table_request;
    vpp_correlated_request_t current_table_request;
    old_context = begin_correlated_request(
            &expired_table_request, &expired_table);
    end_correlated_request(old_context);
    current_context = begin_correlated_request(
            &current_table_request, &current_table);
    vl_api_classify_add_del_table_reply_t table_reply = {0};
    table_reply.context = old_context;
    table_reply.new_table_index = htonl(41);
    vl_api_classify_add_del_table_reply_t_handler(&table_reply);
    if (current_table_request.result_ready || current_table != 99 ||
        get_index_ptr(current_context) !=
            (uintptr_t)&current_table_request)
        return 80;

    table_reply.context = current_context;
    table_reply.new_table_index = htonl(42);
    vl_api_classify_add_del_table_reply_t_handler(&table_reply);
    if (!current_table_request.result_ready || current_table != 42 ||
        get_index_ptr(current_context))
        return 81;

    return 0;
}
