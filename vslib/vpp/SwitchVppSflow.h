#pragma once

#include <saitypes.h>
#include <saisamplepacket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sflow_samplepacket_obj_{
    // Keep for now, persistent objects needed so may move to map
    // std::map<sai_object_id_t, sflow_samplepacket_obj_t> m_samplepacket_objs;
    sai_object_id_t         sample_id;
    
    sai_uint32_t            sample_rate;
    sai_samplepacket_type_t type;
    sai_samplepacket_mode_t mode;
} sflow_samplepacket_obj_t;

#ifdef __cplusplus
}
#endif