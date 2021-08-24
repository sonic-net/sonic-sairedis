#include "Meta.h"
#include "MetaTestSaiInterface.h"

namespace TestLegacy
{
    extern std::shared_ptr<saimeta::MetaTestSaiInterface> g_sai;
    extern std::shared_ptr<saimeta::Meta> g_meta;

    // STATIC HELPER METHODS

    void clear_local();

    sai_object_id_t create_switch();

    void remove_switch(
            _In_ sai_object_id_t switchId);

    sai_object_id_t create_bridge(
            _In_ sai_object_id_t switch_id);

    sai_object_id_t create_port(
            _In_ sai_object_id_t switch_id);

    sai_object_id_t create_bridge_port(
            _In_ sai_object_id_t switch_id,
            _In_ sai_object_id_t bridge_id);

    sai_object_id_t create_dummy_object_id(
            _In_ sai_object_type_t object_type,
            _In_ sai_object_id_t switch_id);

    sai_object_id_t create_virtual_router(
            _In_ sai_object_id_t switch_id);

    sai_object_id_t create_rif(
            _In_ sai_object_id_t switch_id);

    sai_object_id_t create_stp(
            _In_ sai_object_id_t switch_id);

    sai_object_id_t create_next_hop(
            _In_ sai_object_id_t switch_id);
}
