/* Exercise private reply handlers without opening a live VPP API socket. */
#include "../../vslib/vpp/vppxlate/SaiVppXlate.c"

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
    return vat_main.result_ready ? 9 : 0;
}
