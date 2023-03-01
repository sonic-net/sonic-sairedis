#include "sai_vs.h"

VS_GENERIC_QUAD(DASH_ACL_GROUP, dash_acl_group);
VS_BULK_CREATE(DASH_ACL_GROUP, dash_acl_groups);
VS_BULK_REMOVE(DASH_ACL_GROUP, dash_acl_groups);

VS_GENERIC_QUAD(DASH_ACL_RULE, dash_acl_rule);
VS_BULK_CREATE(DASH_ACL_RULE, dash_acl_rules);
VS_BULK_REMOVE(DASH_ACL_RULE, dash_acl_rules);

const sai_dash_acl_api_t vs_dash_acl_api = {
    VS_GENERIC_QUAD_API(dash_acl_group)
    vs_bulk_create_dash_acl_groups,
    vs_bulk_remove_dash_acl_groups,

    VS_GENERIC_QUAD_API(dash_acl_rule)
    vs_bulk_create_dash_acl_rules,
    vs_bulk_remove_dash_acl_rules,
};
