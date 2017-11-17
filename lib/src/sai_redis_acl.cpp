#include "sai_redis.h"

REDIS_GENERIC_QUAD(ACL_TABLE,acl_table);
REDIS_GENERIC_QUAD(ACL_ENTRY,acl_entry);
REDIS_GENERIC_QUAD(ACL_COUNTER,acl_counter);
REDIS_GENERIC_QUAD(ACL_RANGE,acl_range);
REDIS_GENERIC_QUAD(ACL_TABLE_GROUP,acl_table_group);
REDIS_GENERIC_QUAD(ACL_TABLE_GROUP_MEMBER,acl_table_group_member);

const sai_acl_api_t redis_acl_api = {

    REDIS_GENERIC_QUAD_API(acl_table)
    REDIS_GENERIC_QUAD_API(acl_entry)
    REDIS_GENERIC_QUAD_API(acl_counter)
    REDIS_GENERIC_QUAD_API(acl_range)
    REDIS_GENERIC_QUAD_API(acl_table_group)
    REDIS_GENERIC_QUAD_API(acl_table_group_member)
};
