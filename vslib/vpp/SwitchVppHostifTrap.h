#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CoPP: SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP / SAI_OBJECT_TYPE_HOSTIF_TRAP
 * bookkeeping shared between SwitchVpp.h and SwitchVppHostifTrap.cpp.
 */

typedef struct _vpp_trap_group_entry_ {
    // SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE / _QUEUE / _POLICER.
    bool admin_state;
    uint32_t queue;
    sai_object_id_t policer_oid;
} vpp_trap_group_entry_t;

typedef struct _vpp_trap_entry_ {
    // SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE / _PACKET_ACTION / _TRAP_GROUP.
    sai_hostif_trap_type_t trap_type;
    sai_packet_action_t packet_action;
    sai_object_id_t trap_group_oid;

    // Set once the classify/punt binding for this trap_type has been
    // installed in VPP
    bool classify_installed;
} vpp_trap_entry_t;

#ifdef __cplusplus
}
#endif
