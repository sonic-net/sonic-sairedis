#include "Sai.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    if (strcmp(variable, SAI_KEY_VS_SWITCH_TYPE) == 0)
        return SAI_VALUE_VS_SWITCH_TYPE_BCM56850;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};


TEST(Sai, bulkGet)
{
    Sai sai;

    sai.apiInitialize(0, &test_services);

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    auto portNum = attr.value.u32;

    std::vector<sai_object_id_t> oids(portNum);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portNum;
    attr.value.objlist.list = oids.data();
    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    std::vector<sai_attribute_t> attrs(portNum);
    std::vector<uint32_t> attrCounts(portNum, 1);
    std::vector<sai_status_t> statuses(portNum);
    std::vector<sai_attribute_t*> pattrs(portNum);
    for (size_t i = 0; i < portNum; i++)
    {
        attrs[i].id = SAI_PORT_ATTR_ADMIN_STATE;
        pattrs[i] = &attrs[i];
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                portNum,
                oids.data(),
                attrCounts.data(),
                pattrs.data(),
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses.data()));
}