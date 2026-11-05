/* Exercise private reply handlers without opening a live VPP API socket. */
#undef NDEBUG
#include "../../vslib/vpp/vppxlate/SaiVppXlate.c"

static void *request_message;
static int reply_status;
static int check_cache_locks;
static unsigned int cache_updates;
static unsigned int unlocked_cache_updates;

void *__real__hash_set3(void *v, uword key, void *value, void *old_value);
void *__real__hash_unset(void *v, uword key, void *old_value);

static void *probe_cache_locks(void *arg)
{
    unsigned int *unlocked = arg;
    pthread_mutex_t *locks[] = { &vpp_mutex, &vpp_intf_table_mutex };

    for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); ++i) {
        int rc = pthread_mutex_trylock(locks[i]);
        if (rc == 0) {
            ++*unlocked;
            assert(pthread_mutex_unlock(locks[i]) == 0);
        } else {
            assert(rc == EBUSY);
        }
    }
    return NULL;
}

static unsigned int count_unlocked_cache_locks(void)
{
    pthread_t thread;
    unsigned int unlocked = 0;

    /* A different thread must probe the recursive VPP mutex. */
    assert(pthread_create(&thread, NULL, probe_cache_locks, &unlocked) == 0);
    assert(pthread_join(thread, NULL) == 0);
    return unlocked;
}

static void check_cache_update(void)
{
    if (check_cache_locks) {
        ++cache_updates;
        if (count_unlocked_cache_locks())
            ++unlocked_cache_updates;
    }
}

void *__wrap__hash_set3(void *v, uword key, void *value, void *old_value)
{
    check_cache_update();
    return __real__hash_set3(v, key, value, old_value);
}

void *__wrap__hash_unset(void *v, uword key, void *old_value)
{
    check_cache_update();
    return __real__hash_unset(v, key, old_value);
}

void *__wrap_vl_socket_client_msg_alloc(int nbytes)
{
    assert(request_message == NULL);
    request_message = calloc(1, nbytes);
    assert(request_message != NULL);
    return request_message;
}

int __wrap_vl_socket_client_write(void)
{
    return 0;
}

int __wrap_vl_socket_client_read(int wait)
{
    vl_api_create_subif_t *request = request_message;
    assert(request != NULL);
    if (ntohs(request->_vl_msg_id) == VL_API_CREATE_SUBIF) {
        vl_api_create_subif_reply_t reply = {0};
        reply.context = request->context;
        reply.retval = htonl((uint32_t)reply_status);
        reply.sw_if_index = htonl(139);
        vl_api_create_subif_reply_t_handler(&reply);
    } else {
        assert(ntohs(request->_vl_msg_id) == VL_API_DELETE_SUBIF);
        vl_api_delete_subif_reply_t reply = {0};
        reply.retval = htonl((uint32_t)reply_status);
        vl_api_delete_subif_reply_t_handler(&reply);
    }
    free(request_message);
    request_message = NULL;
    return 0;
}

void saivpp_log_info(const char *func, const char *fmt, ...)
{
}

void saivpp_log_error(const char *func, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s: ", func);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

static int test_subinterface_cache(void)
{
    socket_client_main_t socket = {0};
    socket.socket_enable = 1;
    vat_main.socket_client_main = &socket;
    interface_msg_id_base = 0;
    clib_time_init(&vat_main.clib_time);
    clib_mem_init(0, 32 << 20);
    vpp_mutex_lock_init();

    vat_main.sw_if_index_by_interface_name = hash_create_string(0, sizeof(uword));
    interface_name_by_sw_index = hash_create(0, sizeof(uword));
    u8 *parent = format(0, "bobm16%c", 0);
    hash_set_mem(vat_main.sw_if_index_by_interface_name, parent, 16);
    hash_set(interface_name_by_sw_index, 16, parent);
    check_cache_locks = 1;

    assert(create_sub_interface("bobm16", 50, 50) == 0);
    assert(cache_updates == 2);
    assert(get_swif_idx(&vat_main, "bobm16.50") == 139);
    assert(strcmp((char *)hash_get(interface_name_by_sw_index, 139)[0], "bobm16.50") == 0);
    assert(count_unlocked_cache_locks() == 2);

    reply_status = -5;
    assert(delete_sub_interface("bobm16", 50) == -5);
    assert(cache_updates == 2);
    assert(get_swif_idx(&vat_main, "bobm16.50") == 139);
    assert(count_unlocked_cache_locks() == 2);

    reply_status = 0;
    assert(delete_sub_interface("bobm16", 50) == 0);
    assert(cache_updates == 4);
    assert(get_swif_idx(&vat_main, "bobm16.50") == (u32)~0);
    assert(hash_get(interface_name_by_sw_index, 139) == NULL);
    assert(count_unlocked_cache_locks() == 2);

    reply_status = -5;
    assert(create_sub_interface("bobm16", 50, 50) == -5);
    assert(cache_updates == 4);
    assert(get_swif_idx(&vat_main, "bobm16.50") == (u32)~0);
    assert(count_unlocked_cache_locks() == 2);

    check_cache_locks = 0;
    hash_free(vat_main.sw_if_index_by_interface_name);
    hash_free(interface_name_by_sw_index);
    vec_free(parent);
    if (unlocked_cache_updates) {
        fprintf(stderr, "%u of %u subinterface cache updates were not protected by both locks\n",
                unlocked_cache_updates, cache_updates);
        return 10;
    }
    return 0;
}

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
    return vat_main.result_ready ? 9 : test_subinterface_cache();
}
