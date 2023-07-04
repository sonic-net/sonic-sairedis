#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <set>
#include <thread>
#include <algorithm>

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <net/ethernet.h>

#include <swss/logger.h>

#include "Sai.h"
#include "Syncd.h"
#include "MetadataLogger.h"

#include "TestSyncdLib.h"

class SyncdNvdaBfTest : public ::testing::Test
{
public:
    SyncdNvdaBfTest() = default;
    virtual ~SyncdNvdaBfTest() = default;

public:
    virtual void SetUp() override;

    virtual void TearDown() override;

    sai_object_id_t CreateCounter();
    void RemoveCounter(sai_object_id_t counter);

    sai_object_id_t CreateVnet(uint32_t vni);
    void RemoveVnet(sai_object_id_t vnet);

    sai_object_id_t CreateEni(sai_object_id_t vnet);
    void RemoveEni(sai_object_id_t eni);

protected:
    std::shared_ptr<std::thread> m_worker;
    std::shared_ptr<sairedis::Sai> m_sairedis;

    sai_object_id_t m_switchId = SAI_NULL_OBJECT_ID;
};
