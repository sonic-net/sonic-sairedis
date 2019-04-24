#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(TABLE_BITMAP_CLASSIFICATION_ENTRY,table_bitmap_classification_entry);
VS_GENERIC_STATS(TABLE_BITMAP_CLASSIFICATION_ENTRY,table_bitmap_classification_entry);
VS_GENERIC_QUAD(TABLE_BITMAP_ROUTER_ENTRY,table_bitmap_router_entry);
VS_GENERIC_STATS(TABLE_BITMAP_ROUTER_ENTRY,table_bitmap_router_entry);
VS_GENERIC_QUAD(TABLE_META_TUNNEL_ENTRY,table_meta_tunnel_entry);
VS_GENERIC_STATS(TABLE_META_TUNNEL_ENTRY,table_meta_tunnel_entry);

const sai_bmtor_api_t vs_bmtor_api = {
    VS_GENERIC_QUAD_API(table_bitmap_classification_entry)
    VS_GENERIC_STATS_API(table_bitmap_classification_entry)
    VS_GENERIC_QUAD_API(table_bitmap_router_entry)
    VS_GENERIC_STATS_API(table_bitmap_router_entry)
    VS_GENERIC_QUAD_API(table_meta_tunnel_entry)
    VS_GENERIC_STATS_API(table_meta_tunnel_entry)
};
