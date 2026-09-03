#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CoPP: SAI_OBJECT_TYPE_POLICER bookkeeping shared between SwitchVpp.h and
 * SwitchVppPolicer.cpp.
 */

typedef struct _vpp_policer_entry_ {
    // VPP-side policer identity: the name we registered it under and the
    // index VPP assigned on policer_add (needed for del/dump).
    char vpp_name[64];
    uint32_t vpp_policer_index;
} vpp_policer_entry_t;

#ifdef __cplusplus
}
#endif
