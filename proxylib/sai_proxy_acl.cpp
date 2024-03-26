#include "sai_proxy.h"

PROXY_GENERIC_QUAD(ACL_TABLE,acl_table);
PROXY_GENERIC_QUAD(ACL_ENTRY,acl_entry);
PROXY_GENERIC_QUAD(ACL_COUNTER,acl_counter);
PROXY_GENERIC_QUAD(ACL_RANGE,acl_range);
PROXY_GENERIC_QUAD(ACL_TABLE_GROUP,acl_table_group);
PROXY_GENERIC_QUAD(ACL_TABLE_GROUP_MEMBER,acl_table_group_member);
PROXY_GENERIC_QUAD(ACL_TABLE_CHAIN_GROUP,acl_table_chain_group)

const sai_acl_api_t proxy_acl_api = {

    PROXY_GENERIC_QUAD_API(acl_table)
    PROXY_GENERIC_QUAD_API(acl_entry)
    PROXY_GENERIC_QUAD_API(acl_counter)
    PROXY_GENERIC_QUAD_API(acl_range)
    PROXY_GENERIC_QUAD_API(acl_table_group)
    PROXY_GENERIC_QUAD_API(acl_table_group_member)
    PROXY_GENERIC_QUAD_API(acl_table_chain_group)
};
