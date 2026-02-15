#include <gtest/gtest.h>

#include <iostream>

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);

    const auto env = new ::testing::Environment();

    testing::AddGlobalTestEnvironment(env);

    return RUN_ALL_TESTS();
}
