#include "sai_stub.h"

STUB_GENERIC_QUAD(ACL_TABLE,acl_table);
STUB_GENERIC_QUAD(ACL_ENTRY,acl_entry);
STUB_GENERIC_QUAD(ACL_COUNTER,acl_counter);
STUB_GENERIC_QUAD(ACL_RANGE,acl_range);
STUB_GENERIC_QUAD(ACL_TABLE_GROUP,acl_table_group);
STUB_GENERIC_QUAD(ACL_TABLE_GROUP_MEMBER,acl_table_group_member);
STUB_GENERIC_QUAD(ACL_TABLE_CHAIN_GROUP,acl_table_chain_group)

const sai_acl_api_t stub_acl_api = {

    STUB_GENERIC_QUAD_API(acl_table)
    STUB_GENERIC_QUAD_API(acl_entry)
    STUB_GENERIC_QUAD_API(acl_counter)
    STUB_GENERIC_QUAD_API(acl_range)
    STUB_GENERIC_QUAD_API(acl_table_group)
    STUB_GENERIC_QUAD_API(acl_table_group_member)
    STUB_GENERIC_QUAD_API(acl_table_chain_group)
};
