#ifndef __SAI_VS_STATE__
#define __SAI_VS_STATE__

#include <unordered_map>
#include <string>

#include "sai_meta.h"

#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

typedef std::unordered_map<std::string, std::string> AttrHash;
typedef std::unordered_map<std::string, AttrHash> ObjectHash;

// first key is serialized object ID (object id, vlan, route, neighbor, fdb, trap)
// value is hash containing attributes
// second key is serialized attribute ID for given object
// value is serialized attribute value
extern ObjectHash g_objectHash;

extern void reset_id_counter();

extern sai_object_id_t vs_create_real_object_id(
        _In_ sai_object_type_t object_type);

extern sai_status_t init_switch();

extern sai_object_id_t switch_object_id;

extern std::map<sai_vlan_id_t, std::set<sai_object_id_t>> vlan_members_map;
extern void update_vlan_member_list_on_vlan(
        _In_ sai_vlan_id_t vlan_id);

#endif // __SAI_VS_STATE__
