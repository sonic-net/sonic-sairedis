#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry);
REDIS_BULK_CREATE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);
REDIS_BULK_REMOVE_ENTRY_EX(ENI_ETHER_ADDRESS_MAP_ENTRY, eni_ether_address_map_entry, eni_ether_address_map_entries);

REDIS_GENERIC_QUAD(ENI, eni);
REDIS_BULK_CREATE(ENI, enis);
REDIS_BULK_REMOVE(ENI, enis);

const sai_dash_eni_api_t redis_dash_eni_api = {
    REDIS_GENERIC_QUAD_API(eni_ether_address_map_entry)
    redis_bulk_create_eni_ether_address_map_entries,
    redis_bulk_remove_eni_ether_address_map_entries,

    REDIS_GENERIC_QUAD_API(eni)
    redis_bulk_create_enis,
    redis_bulk_remove_enis,
};
