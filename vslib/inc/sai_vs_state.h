#ifndef __SAI_VS_STATE__
#define __SAI_VS_STATE__

#include <unordered_map>
#include <string>

#include "sai_meta.h"

#include "saiserialize.h"
#include "saiattributelist.h"

typedef std::unordered_map<std::string, std::string> AttrHash;
typedef std::unordered_map<std::string, AttrHash> ObjectHash;

// first key is serialized object ID (object id, vlan, route, neighbor, fdb, trap)
// value is hash containing attributes
// second key is serialized attribute ID for given object
// value is serialized attribute value
extern ObjectHash g_objectHash;

extern void reset_id_counter();

extern sai_object_id_t switch_object_id;

extern sai_status_t init_switch();

#endif // __SAI_VS_STATE__
