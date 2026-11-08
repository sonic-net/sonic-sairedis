#pragma once

extern "C" {
    #include <sai.h>
    #include <saiextensions.h>
}

#include "gtest/gtest.h"

#define ENV_CLIENT_MODE_OPT "TESTDASH_CLIENT_MODE"

class TestDashEnv : public ::testing::Environment
{
public:
    static TestDashEnv* instance();

    virtual void SetUp() override;

    virtual void TearDown() override;

    sai_object_id_t getSwitchOid() const { return m_switch_id; }

private:
    void StopSyncd();
    void StartSyncd();
    void FlushRedis();
    void CreateSwitch();
    TestDashEnv();

    sai_object_id_t m_switch_id = SAI_NULL_OBJECT_ID;
    bool m_manage_syncd = true;
};
