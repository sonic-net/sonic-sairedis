#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "Sai.h"

using namespace saiproxy;

class SaiTest : public ::testing::Test
{
public:
    SaiTest() = default;
    virtual ~SaiTest() = default;

public:
    virtual void SetUp() override
    {
        m_sai = std::make_shared<Sai>();

        //sai_attribute_t attr;
        //attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        //attr.value.booldata = true;

        //auto status = m_sai->create(SAI_OBJECT_TYPE_SWITCH, &m_swid, SAI_NULL_OBJECT_ID, 1, &attr);
        //ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        //auto status = m_sai->remove(SAI_OBJECT_TYPE_SWITCH, m_swid);
        //ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

protected:
    std::shared_ptr<Sai> m_sai;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;

    const std::uint32_t m_guid = 0; // default context config id
    const std::uint32_t m_scid = 0; // default switch config id
};

TEST_F(SaiTest, Ctr)
{
    auto s = std::make_shared<Sai>();
}
