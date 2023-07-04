#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry);
VS_BULK_CREATE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);
VS_BULK_REMOVE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);

VS_GENERIC_QUAD(ENI, eni);
VS_BULK_CREATE(ENI, enis);
VS_BULK_REMOVE(ENI, enis);

const sai_dash_eni_api_t vs_dash_eni_api = {
    VS_GENERIC_QUAD_API(eni_ether_address_map_entry)
    vs_bulk_create_eni_ether_address_map_entries,
    vs_bulk_remove_eni_ether_address_map_entries,

    VS_GENERIC_QUAD_API(eni)
    vs_bulk_create_enis,
    vs_bulk_remove_enis,
};
