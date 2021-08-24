#include "TestLegacy.h"

#include "sai_serialize.h"

#include <gtest/gtest.h>

//using namespace TestLegacy;

using namespace saimeta;

namespace TestLegacy
{
    std::shared_ptr<MetaTestSaiInterface> g_sai = std::make_shared<saimeta::MetaTestSaiInterface>();
    std::shared_ptr<Meta> g_meta = std::make_shared<saimeta::Meta>(g_sai);

    // STATIC HELPER METHODS

    void clear_local()
    {
        SWSS_LOG_ENTER();

        g_sai = std::make_shared<MetaTestSaiInterface>();
        g_meta = std::make_shared<Meta>(g_sai);
    }

    sai_object_id_t create_switch()
    {
        SWSS_LOG_ENTER();

        sai_attribute_t attr;

        sai_object_id_t switch_id;

        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        auto status = g_meta->create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr);
        EXPECT_EQ(SAI_STATUS_SUCCESS, status);

        return switch_id;
    }

    void remove_switch(
            _In_ sai_object_id_t switchId)
    {
        SWSS_LOG_ENTER();

        EXPECT_TRUE(g_meta->isEmpty() == false);

        SWSS_LOG_NOTICE("removing: %lX", switchId);

        auto status = g_meta->remove(SAI_OBJECT_TYPE_SWITCH, switchId);

        EXPECT_EQ(SAI_STATUS_SUCCESS, status);

        EXPECT_TRUE(g_meta->isEmpty());
    }

    sai_object_id_t create_bridge(
            _In_ sai_object_id_t switch_id)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t bridge_id;

        sai_attribute_t attrs[9] = {};

        attrs[0].id = SAI_BRIDGE_ATTR_TYPE;
        attrs[0].value.s32 = SAI_BRIDGE_TYPE_1Q;

        sai_status_t status = g_meta->create(SAI_OBJECT_TYPE_BRIDGE, &bridge_id, switch_id, 1, attrs);
        EXPECT_EQ(SAI_STATUS_SUCCESS, status);

        return bridge_id;
    }

    sai_object_id_t create_port(
            _In_ sai_object_id_t switch_id)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t port;

        static uint32_t id = 1;
        id++;
        sai_attribute_t attrs[9] = { };

        uint32_t list[1] = { id };

        attrs[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
        attrs[0].value.u32list.count = 1;
        attrs[0].value.u32list.list = list;

        attrs[1].id = SAI_PORT_ATTR_SPEED;
        attrs[1].value.u32 = 10000;

        auto status = g_meta->create(SAI_OBJECT_TYPE_PORT, &port, switch_id, 2, attrs);
        EXPECT_EQ(SAI_STATUS_SUCCESS, status);

        return port;
    }

    sai_object_id_t create_bridge_port(
            _In_ sai_object_id_t switch_id,
            _In_ sai_object_id_t bridge_id)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t bridge_port;

        sai_attribute_t attrs[9] = { };

        auto port = create_port(switch_id);

        attrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
        attrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;

        attrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
        attrs[1].value.oid = port;

        auto status = g_meta->create(SAI_OBJECT_TYPE_BRIDGE_PORT, &bridge_port, switch_id, 2, attrs);
        EXPECT_EQ(SAI_STATUS_SUCCESS, status);

        return bridge_port;
    }

    sai_object_id_t create_dummy_object_id(
            _In_ sai_object_type_t object_type,
            _In_ sai_object_id_t switch_id)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t oid;

        auto status = g_sai->create(object_type, &oid, switch_id, 0, NULL);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to create oid");
        }

        SWSS_LOG_DEBUG("created oid %s", sai_serialize_object_id(oid).c_str());

        return oid;
    }

    sai_object_id_t create_virtual_router(
            _In_ sai_object_id_t switch_id)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t vr;

        auto status = g_meta->create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vr, switch_id, 0, NULL);
        EXPECT_EQ(SAI_STATUS_SUCCESS, status);

        return vr;
    }

}
