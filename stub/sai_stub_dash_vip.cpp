#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(VIP_ENTRY, vip_entry);
STUB_BULK_CREATE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);
STUB_BULK_REMOVE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);

const sai_dash_vip_api_t stub_dash_vip_api = {
    STUB_GENERIC_QUAD_API(vip_entry)
    stub_bulk_create_vip_entries,
    stub_bulk_remove_vip_entries,
};
