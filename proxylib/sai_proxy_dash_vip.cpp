#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(VIP_ENTRY, vip_entry);
PROXY_BULK_CREATE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);
PROXY_BULK_REMOVE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);

const sai_dash_vip_api_t proxy_dash_vip_api = {
    PROXY_GENERIC_QUAD_API(vip_entry)
    proxy_bulk_create_vip_entries,
    proxy_bulk_remove_vip_entries,
};
