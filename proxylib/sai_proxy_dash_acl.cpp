#include "sai_proxy.h"

PROXY_GENERIC_QUAD(DASH_ACL_GROUP, dash_acl_group);
PROXY_BULK_CREATE(DASH_ACL_GROUP, dash_acl_groups);
PROXY_BULK_REMOVE(DASH_ACL_GROUP, dash_acl_groups);

PROXY_GENERIC_QUAD(DASH_ACL_RULE, dash_acl_rule);
PROXY_BULK_CREATE(DASH_ACL_RULE, dash_acl_rules);
PROXY_BULK_REMOVE(DASH_ACL_RULE, dash_acl_rules);

const sai_dash_acl_api_t proxy_dash_acl_api = {
    PROXY_GENERIC_QUAD_API(dash_acl_group)
    proxy_bulk_create_dash_acl_groups,
    proxy_bulk_remove_dash_acl_groups,

    PROXY_GENERIC_QUAD_API(dash_acl_rule)
    proxy_bulk_create_dash_acl_rules,
    proxy_bulk_remove_dash_acl_rules,
};
