#include "vpp/VppTransaction.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace saivs;

namespace
{
    // Records an undo that appends its name to the log and returns the given code
    void record(
            std::vector<std::string>& log,
            const std::string& name,
            int ret = 0)
    {
        VppTransaction::current().onRollback(name, [&log, name, ret]()
                {
                    log.push_back(name);
                    return ret;
                });
    }
}

TEST(VppTransaction, inactiveWithoutScope)
{
    std::vector<std::string> log;

    EXPECT_FALSE(VppTransaction::current().active());

    int calls = 0;
    EXPECT_EQ(0, VppTransaction::current().run("step",
                [&]() { ++calls; return 0; },
                [&]() { log.push_back("step"); return 0; }));
    EXPECT_EQ(1, calls);

    record(log, "dropped");
    EXPECT_EQ(0u, VppTransaction::current().savepoint());
    EXPECT_TRUE(log.empty());
}

TEST(VppTransaction, rollsBackNewestFirst)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        EXPECT_TRUE(VppTransaction::current().active());
        record(log, "a");
        record(log, "b");
        record(log, "c");
        EXPECT_EQ(SAI_STATUS_SUCCESS, scope.rollback());
    }

    EXPECT_EQ((std::vector<std::string>{"c", "b", "a"}), log);
    EXPECT_FALSE(VppTransaction::current().active());
}

TEST(VppTransaction, commitDropsUndo)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        record(log, "a");
        scope.commit();
    }

    EXPECT_TRUE(log.empty());
}

TEST(VppTransaction, scopeLeftWithoutCommitRollsBack)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        record(log, "a");
    }

    EXPECT_EQ((std::vector<std::string>{"a"}), log);
}

TEST(VppTransaction, failingUndoDoesNotStopTheOthers)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        record(log, "a");
        record(log, "b", -1);
        scope.txn().onRollback("c", [&log]() -> int
                {
                    log.push_back("c");
                    throw std::runtime_error("undo threw");
                });
        EXPECT_EQ(SAI_STATUS_FAILURE, scope.rollback());
    }

    EXPECT_EQ((std::vector<std::string>{"c", "b", "a"}), log);
}

TEST(VppTransaction, runRecordsOnlyOnSuccess)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        EXPECT_EQ(-5, scope.txn().run("failed", []() { return -5; },
                    [&log]() { log.push_back("failed"); return 0; }));
        EXPECT_EQ(0, scope.txn().run("done", []() { return 0; },
                    [&log]() { log.push_back("done"); return 0; }));
        EXPECT_EQ(1u, scope.txn().savepoint());
    }

    EXPECT_EQ((std::vector<std::string>{"done"}), log);
}

TEST(VppTransaction, nestedScopeIsASavepoint)
{
    std::vector<std::string> log;

    {
        TxnScope outer;
        record(log, "outer");

        {
            TxnScope failed;
            EXPECT_EQ(&outer.txn(), &failed.txn());
            record(log, "failed");
        }
        EXPECT_EQ((std::vector<std::string>{"failed"}), log);

        {
            TxnScope committed;
            record(log, "committed");
            committed.commit();
        }
        EXPECT_EQ(2u, outer.txn().savepoint());

        EXPECT_EQ(SAI_STATUS_SUCCESS, outer.rollback());
    }

    EXPECT_EQ((std::vector<std::string>{"failed", "committed", "outer"}), log);
}

TEST(VppTransaction, undoDoesNotRecordWhileRollingBack)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        scope.txn().onRollback("a", [&log]()
                {
                    log.push_back("a");
                    record(log, "late");
                    return 0;
                });
        EXPECT_EQ(SAI_STATUS_SUCCESS, scope.rollback());
        EXPECT_EQ(0u, scope.txn().savepoint());
    }

    EXPECT_EQ((std::vector<std::string>{"a"}), log);
}

TEST(VppTransaction, scopeOpenedByAnUndoOwnsItsTransaction)
{
    std::vector<std::string> log;

    {
        TxnScope scope;
        record(log, "a");
        scope.txn().onRollback("b", [&log, &scope]()
                {
                    log.push_back("b");

                    {
                        TxnScope helper;
                        EXPECT_NE(&scope.txn(), &helper.txn());
                        record(log, "helper failed");
                    }

                    TxnScope helper;
                    record(log, "helper done");
                    helper.commit();
                    return 0;
                });
        EXPECT_EQ(SAI_STATUS_SUCCESS, scope.rollback());
        EXPECT_EQ(&scope.txn(), &VppTransaction::current());
        EXPECT_EQ(0u, scope.txn().savepoint());
    }

    EXPECT_EQ((std::vector<std::string>{"b", "helper failed", "a"}), log);
}

TEST(VppTransaction, eachThreadHasItsOwn)
{
    std::vector<std::string> mainLog;
    std::vector<std::string> threadLog;

    TxnScope scope;
    record(mainLog, "main");

    std::thread other([&threadLog]()
            {
                EXPECT_FALSE(VppTransaction::current().active());

                TxnScope otherScope;
                record(threadLog, "other");
                EXPECT_EQ(1u, otherScope.txn().savepoint());
                EXPECT_EQ(SAI_STATUS_SUCCESS, otherScope.rollback());
            });
    other.join();

    EXPECT_EQ((std::vector<std::string>{"other"}), threadLog);
    EXPECT_TRUE(mainLog.empty());
    EXPECT_EQ(1u, scope.txn().savepoint());

    EXPECT_EQ(SAI_STATUS_SUCCESS, scope.rollback());
    EXPECT_EQ((std::vector<std::string>{"main"}), mainLog);
}
