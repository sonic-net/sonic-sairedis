#include "sai_redis.h"

REDIS_GENERIC_QUAD(DASH_ACL_GROUP, dash_acl_group);
REDIS_BULK_CREATE(DASH_ACL_GROUP, dash_acl_groups);
REDIS_BULK_REMOVE(DASH_ACL_GROUP, dash_acl_groups);

REDIS_GENERIC_QUAD(DASH_ACL_RULE, dash_acl_rule);
REDIS_BULK_CREATE(DASH_ACL_RULE, dash_acl_rules);
REDIS_BULK_REMOVE(DASH_ACL_RULE, dash_acl_rules);

const sai_dash_acl_api_t redis_dash_acl_api = {
    REDIS_GENERIC_QUAD_API(dash_acl_group)
    redis_bulk_create_dash_acl_groups,
    redis_bulk_remove_dash_acl_groups,

    REDIS_GENERIC_QUAD_API(dash_acl_rule)
    redis_bulk_create_dash_acl_rules,
    redis_bulk_remove_dash_acl_rules,
};
