#include "Asan.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unistd.h>

namespace
{

struct AsanSignalState
{
    // When true, mock_signal returns SIG_ERR.
    bool fail = false;

    int calls = 0;
    int last_sig = 0;
    // Last handler passed to signal() (init install or handler restore).
    sighandler_t last_set = SIG_DFL;
};

struct AsanTestState
{
    AsanSignalState signal;

    int access_calls = 0;
    int access_rc = -1;
    std::string access_path;

    int malloc_calls = 0;
    size_t malloc_size = 0;
    // When true, mock_malloc returns nullptr. Otherwise it returns storage.data().
    bool malloc_fail = false;
    std::vector<unsigned char> storage;

    int leak_check_calls = 0;

    int raise_calls = 0;
    int raise_signo = -1;
};

AsanTestState *g_state = nullptr;

sighandler_t mock_signal(int sig, sighandler_t handler)
{
    // SWSS_LOG_ENTER(); // disabled
    EXPECT_NE(g_state, nullptr);
    auto& sigst = g_state->signal;
    sigst.calls++;
    sigst.last_sig = sig;
    sigst.last_set = handler;
    return sigst.fail ? SIG_ERR : SIG_DFL;
}

int mock_access(const char *path, int mode)
{
    // SWSS_LOG_ENTER(); // disabled
    EXPECT_NE(g_state, nullptr);
    EXPECT_EQ(mode, F_OK);
    g_state->access_calls++;
    g_state->access_path = path ? path : "";
    return g_state->access_rc;
}

void *mock_malloc(size_t size)
{
    // SWSS_LOG_ENTER(); // disabled
    EXPECT_NE(g_state, nullptr);
    g_state->malloc_calls++;
    g_state->malloc_size = size;
    if (g_state->malloc_fail)
    {
        return nullptr;
    }
    g_state->storage.assign(size, 0);
    return g_state->storage.data();
}

void mock_leak_check(void)
{
    // SWSS_LOG_ENTER(); // disabled
    EXPECT_NE(g_state, nullptr);
    g_state->leak_check_calls++;
}

int mock_raise(int signo)
{
    // SWSS_LOG_ENTER(); // disabled
    EXPECT_NE(g_state, nullptr);
    g_state->raise_calls++;
    g_state->raise_signo = signo;
    return 0;
}

void invoke_handler_impl()
{
    // SWSS_LOG_ENTER(); // disabled
    asan_sigterm_handler_impl(SIGTERM, mock_leak_check, mock_signal, mock_raise);
}

} // namespace

class AsanInitTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        state_ = {};
        g_state = &state_;
    }

    void TearDown() override
    {
        g_state = nullptr;
    }

    AsanTestState state_;
};

TEST_F(AsanInitTest, InstallsSigtermHandler)
{
    state_.access_rc = -1;

    ASSERT_TRUE(asan_init_impl(mock_signal, mock_access, mock_malloc, mock_leak_check));

    EXPECT_EQ(state_.signal.calls, 1);
    EXPECT_EQ(state_.signal.last_sig, SIGTERM);
    EXPECT_EQ(state_.signal.last_set, asan_sigterm_handler);
    EXPECT_EQ(state_.access_calls, 1);
    EXPECT_EQ(state_.access_path, "/etc/sonic/inject_asan_test_leak_enabled");
    EXPECT_EQ(state_.malloc_calls, 0);
}

TEST_F(AsanInitTest, SignalFailureReturnsFalse)
{
    state_.signal.fail = true;

    EXPECT_FALSE(asan_init_impl(mock_signal, mock_access, mock_malloc, mock_leak_check));

    EXPECT_EQ(state_.signal.calls, 1);
    EXPECT_EQ(state_.access_calls, 0);
    EXPECT_EQ(state_.malloc_calls, 0);
}

TEST_F(AsanInitTest, SkipsLeakInjectionWhenFlagFileMissing)
{
    state_.access_rc = -1;

    ASSERT_TRUE(asan_init_impl(mock_signal, mock_access, mock_malloc, mock_leak_check));

    EXPECT_EQ(state_.malloc_calls, 0);
}

TEST_F(AsanInitTest, InjectsLeakWhenFlagFilePresent)
{
    state_.access_rc = 0;

    ASSERT_TRUE(asan_init_impl(mock_signal, mock_access, mock_malloc, mock_leak_check));

    EXPECT_EQ(state_.malloc_calls, 1);
    EXPECT_EQ(state_.malloc_size, ASAN_TEST_LEAK_SIZE);
    ASSERT_EQ(state_.storage.size(), ASAN_TEST_LEAK_SIZE);
    EXPECT_EQ(state_.storage.front(), static_cast<unsigned char>(0xCD));
    EXPECT_EQ(state_.storage.back(), static_cast<unsigned char>(0xCD));
    EXPECT_EQ(state_.storage[state_.storage.size() / 2], static_cast<unsigned char>(0xCD));
}

TEST_F(AsanInitTest, MallocFailureStillReturnsTrue)
{
    state_.access_rc = 0;
    state_.malloc_fail = true;

    // Injection failure is logged; init itself still succeeds so syncd keeps
    // running with the SIGTERM handler installed.
    ASSERT_TRUE(asan_init_impl(mock_signal, mock_access, mock_malloc, mock_leak_check));

    EXPECT_EQ(state_.malloc_calls, 1);
    EXPECT_EQ(state_.malloc_size, ASAN_TEST_LEAK_SIZE);
    EXPECT_TRUE(state_.storage.empty());
}

TEST(AsanInjectTest, FillsAllocationViaInjectedMalloc)
{
    AsanTestState state;
    g_state = &state;

    asan_inject_test_leak(mock_malloc);

    EXPECT_EQ(state.malloc_calls, 1);
    EXPECT_EQ(state.malloc_size, ASAN_TEST_LEAK_SIZE);
    ASSERT_EQ(state.storage.size(), ASAN_TEST_LEAK_SIZE);
    EXPECT_EQ(state.storage.front(), static_cast<unsigned char>(0xCD));
    EXPECT_EQ(state.storage.back(), static_cast<unsigned char>(0xCD));

    g_state = nullptr;
}

TEST(AsanInjectTest, NullMallocIsANoOp)
{
    AsanTestState state;
    state.malloc_fail = true;
    g_state = &state;

    asan_inject_test_leak(mock_malloc);

    EXPECT_EQ(state.malloc_calls, 1);
    EXPECT_TRUE(state.storage.empty());

    g_state = nullptr;
}

class AsanSigtermHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        state_ = {};
        g_state = &state_;
    }

    void TearDown() override
    {
        g_state = nullptr;
    }

    AsanTestState state_;
};

TEST_F(AsanSigtermHandlerTest, RunsLeakCheckRestoresDefaultAndRaises)
{
    invoke_handler_impl();

    EXPECT_EQ(state_.leak_check_calls, 1);
    EXPECT_EQ(state_.signal.calls, 1);
    EXPECT_EQ(state_.signal.last_sig, SIGTERM);
    EXPECT_EQ(state_.signal.last_set, SIG_DFL);
    EXPECT_EQ(state_.raise_calls, 1);
    EXPECT_EQ(state_.raise_signo, SIGTERM);
}

TEST_F(AsanSigtermHandlerTest, NullLeakCheckDoesNotCrash)
{
    asan_sigterm_handler_impl(SIGTERM, nullptr, mock_signal, mock_raise);

    EXPECT_EQ(state_.leak_check_calls, 0);
    EXPECT_EQ(state_.signal.calls, 1);
    EXPECT_EQ(state_.signal.last_set, SIG_DFL);
    EXPECT_EQ(state_.raise_calls, 1);
    EXPECT_EQ(state_.raise_signo, SIGTERM);
}
