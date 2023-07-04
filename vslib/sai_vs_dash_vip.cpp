#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(VIP_ENTRY, vip_entry);
VS_BULK_CREATE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);
VS_BULK_REMOVE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);

const sai_dash_vip_api_t vs_dash_vip_api = {
    VS_GENERIC_QUAD_API(vip_entry)
    vs_bulk_create_vip_entries,
    vs_bulk_remove_vip_entries,
};
