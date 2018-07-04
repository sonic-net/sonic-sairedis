#include <getopt.h>
#include <unistd.h>

#include "string.h"
extern "C" {
#include "sai.h"
}

#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

void match_list_lengths(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        uint32_t attr_count,
        sai_attribute_t* attr_list);

void match_redis_with_rec(
        sai_object_id_t get_oid,
        sai_object_id_t oid);

void match_redis_with_rec(
        sai_object_list_t get_objlist,
        sai_object_list_t objlist);

void match_redis_with_rec(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        uint32_t attr_count,
        sai_attribute_t* attr_list);

sai_status_t handle_fdb(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

sai_status_t handle_neighbor(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

sai_status_t handle_route(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

sai_object_id_t translate_local_to_redis(
        _In_ sai_object_id_t rid);

void translate_local_to_redis(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);