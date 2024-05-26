#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry);
STUB_BULK_CREATE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);
STUB_BULK_REMOVE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);

STUB_GENERIC_QUAD(ENI, eni);
STUB_GENERIC_STATS(ENI, eni);
STUB_BULK_CREATE(ENI, enis);
STUB_BULK_REMOVE(ENI, enis);

const sai_dash_eni_api_t stub_dash_eni_api = {
    STUB_GENERIC_QUAD_API(eni_ether_address_map_entry)
    stub_bulk_create_eni_ether_address_map_entries,
    stub_bulk_remove_eni_ether_address_map_entries,

    STUB_GENERIC_QUAD_API(eni)
    STUB_GENERIC_STATS_API(eni)
    stub_bulk_create_enis,
    stub_bulk_remove_enis,
};
