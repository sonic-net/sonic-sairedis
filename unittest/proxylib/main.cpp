#include <gtest/gtest.h>

#include <iostream>

class SwsscommonEnvironment:
    public ::testing::Environment
{
    public:
        void SetUp() override { }
};

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);

    const auto env = new SwsscommonEnvironment;

    testing::AddGlobalTestEnvironment(env);

    return RUN_ALL_TESTS();
}
