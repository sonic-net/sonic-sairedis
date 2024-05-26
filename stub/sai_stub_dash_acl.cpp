#include "sai_stub.h"

STUB_GENERIC_QUAD(DASH_ACL_GROUP, dash_acl_group);
STUB_BULK_CREATE(DASH_ACL_GROUP, dash_acl_groups);
STUB_BULK_REMOVE(DASH_ACL_GROUP, dash_acl_groups);

STUB_GENERIC_QUAD(DASH_ACL_RULE, dash_acl_rule);
STUB_BULK_CREATE(DASH_ACL_RULE, dash_acl_rules);
STUB_BULK_REMOVE(DASH_ACL_RULE, dash_acl_rules);

const sai_dash_acl_api_t stub_dash_acl_api = {
    STUB_GENERIC_QUAD_API(dash_acl_group)
    stub_bulk_create_dash_acl_groups,
    stub_bulk_remove_dash_acl_groups,

    STUB_GENERIC_QUAD_API(dash_acl_rule)
    stub_bulk_create_dash_acl_rules,
    stub_bulk_remove_dash_acl_rules,
};
