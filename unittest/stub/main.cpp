#include <gtest/gtest.h>

#include "DummySaiInterface.h"

#include <iostream>

std::shared_ptr<sairedis::SaiInterface> stub_sai;

auto sai = std::make_shared<saimeta::DummySaiInterface>();

class SwsscommonEnvironment:
    public ::testing::Environment
{
    public:
        void SetUp() override {

            sai->setStatus(SAI_STATUS_FAILURE);

            stub_sai = sai;
        }
};

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);

    const auto env = new SwsscommonEnvironment;

    testing::AddGlobalTestEnvironment(env);

    return RUN_ALL_TESTS();
}
