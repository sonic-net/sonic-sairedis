#include "sai_vs.h"

VS_GENERIC_QUAD(ACL_TABLE,acl_table);
VS_GENERIC_QUAD(ACL_ENTRY,acl_entry);
VS_GENERIC_QUAD(ACL_COUNTER,acl_counter);
VS_GENERIC_QUAD(ACL_RANGE,acl_range);
VS_GENERIC_QUAD(ACL_TABLE_GROUP,acl_table_group);
VS_GENERIC_QUAD(ACL_TABLE_GROUP_MEMBER,acl_table_group_member);

const sai_acl_api_t vs_acl_api = {

    VS_GENERIC_QUAD_API(acl_table)
    VS_GENERIC_QUAD_API(acl_entry)
    VS_GENERIC_QUAD_API(acl_counter)
    VS_GENERIC_QUAD_API(acl_range)
    VS_GENERIC_QUAD_API(acl_table_group)
    VS_GENERIC_QUAD_API(acl_table_group_member)
};
