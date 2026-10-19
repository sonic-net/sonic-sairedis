#pragma once

/*
 * Shared harness for the SwitchVpp unit tests (TestSwitchVpp*.cpp).
 *
 * The test binary is linked with the linker's --wrap option for the VPP API
 * calls in VPP_TEST_WRAPS, so SwitchVpp calls land on __wrap_ stubs instead
 * of a VPP socket. MockVppXlate.cpp holds every stub, and the ones a test
 * checks record their calls in vppMock().calls; what name, id and sub_id hold
 * depends on the API, and each stub documents its own.
 *
 * The unit tests are built with access control checking disabled, so
 * fixtures can reach protected SwitchVpp members directly.
 */

#include "MockVppXlate.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace saivs_test
{
    // position of the first recorded call matching api and name, or -1
    int vppCallIndex(
            const std::string& api,
            const std::string& name);

    std::vector<VppCall> vppCallsTo(
            const std::string& api);

    /*
     * A cold-booted VPP switch with the default virtual router and nothing
     * else, a fresh vppMock() and no recorded calls. Router interfaces get no tap devices
     * unless useTapDevice() says so, so they are SAI state only.
     */
    class SwitchVppTest : public ::testing::Test
    {
        protected:

            void SetUp() override;

            void TearDown() override;

            virtual bool useTapDevice() const;

            sai_status_t setSwitch(
                    const sai_attribute_t& attr);

            const sai_object_id_t m_switchId = 0x2100000000;

            std::shared_ptr<saivs::RealObjectIdManager> m_mgr;

            std::shared_ptr<saivs::SwitchVpp> m_sw;

            sai_object_id_t m_defaultVr = SAI_NULL_OBJECT_ID;
    };
}
