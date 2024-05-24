#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry);
PROXY_BULK_CREATE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);
PROXY_BULK_REMOVE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);

PROXY_GENERIC_QUAD(ENI, eni);
PROXY_GENERIC_STATS(ENI, eni);
PROXY_BULK_CREATE(ENI, enis);
PROXY_BULK_REMOVE(ENI, enis);

const sai_dash_eni_api_t proxy_dash_eni_api = {
    PROXY_GENERIC_QUAD_API(eni_ether_address_map_entry)
    proxy_bulk_create_eni_ether_address_map_entries,
    proxy_bulk_remove_eni_ether_address_map_entries,

    PROXY_GENERIC_QUAD_API(eni)
    PROXY_GENERIC_STATS_API(eni)
    proxy_bulk_create_enis,
    proxy_bulk_remove_enis,
};
