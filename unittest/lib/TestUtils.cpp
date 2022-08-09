#include "Utils.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(Utils, clearOidValues)
{
    sai_attribute_t attr;

    sai_object_id_t oids[1];

    attr.id = 1000;

    EXPECT_THROW(Utils::clearOidValues(SAI_OBJECT_TYPE_NULL, 1, &attr), std::runtime_error);

    attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT;

    attr.value.aclfield.data.oid = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(attr.value.aclfield.data.oid, 0);

    attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS;

    attr.value.aclfield.enable = true;

    attr.value.aclfield.data.objlist.count = 1;
    attr.value.aclfield.data.objlist.list = oids;

    oids[0] = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(oids[0], 0);

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_COUNTER;

    attr.value.aclaction.parameter.oid = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(attr.value.aclaction.parameter.oid, 0);

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST;

    attr.value.aclaction.enable = true;

    attr.value.aclaction.parameter.objlist.count = 1;
    attr.value.aclaction.parameter.objlist.list = oids;

    oids[0] = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(oids[0], 0);

}
