#include "Channel.h"
#include "ContextConfigContainer.h"
#include "RedisRemoteSaiInterface.h"
#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <stdexcept>

using namespace sairedis;
using namespace swss;

namespace
{
class BulkResponseChannel : public Channel
{
public:
    BulkResponseChannel() : Channel(nullptr)
    {
        SWSS_LOG_ENTER();
    }

    void setBuffered(bool) override {}
    void flush() override {}
    void set(const std::string&, const std::vector<FieldValueTuple>&, const std::string&) override {}
    void del(const std::string&, const std::string&) override {}

    sai_status_t wait(const std::string& command, KeyOpFieldsValuesTuple& kco) override
    {
        EXPECT_EQ(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, command);
        ++waitCalls;
        kfvFieldsValues(kco) = values;
        return status;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    std::vector<FieldValueTuple> values;
    size_t waitCalls = 0;

protected:
    void notificationThreadFunction() override {}
};

class BulkResponseTest : public ::testing::TestWithParam<uint32_t>
{
protected:
    void SetUp() override
    {
        auto ctx = ContextConfigContainer::loadFromFile("foo");
        remote.reset(new RedisRemoteSaiInterface(ctx->get(0), nullptr, std::make_shared<Recorder>()));
        remote->m_syncMode = true;
        channel = std::make_shared<BulkResponseChannel>();
        remote->m_communicationChannel = channel;
    }

    void setResponse(const std::vector<sai_status_t>& statuses)
    {
        SWSS_LOG_ENTER();

        channel->values.clear();
        for (auto status : statuses)
        {
            channel->values.emplace_back(sai_serialize_status(status), "");
        }
    }

    const std::array<sai_common_api_t, 3> apis = {{
        SAI_COMMON_API_BULK_CREATE,
        SAI_COMMON_API_BULK_SET,
        SAI_COMMON_API_BULK_REMOVE,
    }};
    const std::array<sai_status_t, 2> failedWaitStatuses = {{
        static_cast<sai_status_t>(SAI_STATUS_FAILURE),
        static_cast<sai_status_t>(SAI_STATUS_UNINITIALIZED),
    }};
    const std::array<sai_status_t, 2> aggregateStatuses = {{
        static_cast<sai_status_t>(SAI_STATUS_SUCCESS),
        static_cast<sai_status_t>(SAI_STATUS_FAILURE),
    }};
    std::shared_ptr<BulkResponseChannel> channel;
    std::unique_ptr<RedisRemoteSaiInterface> remote;
};

TEST_P(BulkResponseTest, FailedWaitWithoutFieldsReturnsFailure)
{
    for (auto api : apis)
    {
        for (auto status : failedWaitStatuses)
        {
            SCOPED_TRACE(sai_serialize_common_api(api));
            channel->status = status;
            channel->values.clear();
            std::vector<sai_status_t> objectStatuses(GetParam(), SAI_STATUS_SUCCESS);

            EXPECT_EQ(status, remote->waitForBulkResponse(api, GetParam(), objectStatuses.data()));
            EXPECT_EQ(std::vector<sai_status_t>(GetParam(), status), objectStatuses);
        }
    }
}

TEST_P(BulkResponseTest, MalformedResponsesRemainRejected)
{
    for (auto api : apis)
    {
        SCOPED_TRACE(sai_serialize_common_api(api));
        std::vector<sai_status_t> objectStatuses(GetParam(), SAI_STATUS_NOT_EXECUTED);

        channel->status = SAI_STATUS_SUCCESS;
        channel->values.clear();
        EXPECT_THROW(
                remote->waitForBulkResponse(api, GetParam(), objectStatuses.data()),
                std::runtime_error);

        channel->status = SAI_STATUS_FAILURE;
        setResponse(std::vector<sai_status_t>(GetParam() + 1, SAI_STATUS_FAILURE));
        EXPECT_THROW(
                remote->waitForBulkResponse(api, GetParam(), objectStatuses.data()),
                std::runtime_error);
    }
}

TEST_P(BulkResponseTest, CompleteResponsesRemainUnchanged)
{
    for (auto api : apis)
    {
        for (auto aggregateStatus : aggregateStatuses)
        {
            SCOPED_TRACE(sai_serialize_common_api(api));
            std::vector<sai_status_t> expected(GetParam(), SAI_STATUS_INVALID_PARAMETER);
            expected.front() = SAI_STATUS_SUCCESS;
            if (GetParam() > 1)
            {
                expected.back() = SAI_STATUS_NOT_EXECUTED;
            }
            channel->status = aggregateStatus;
            setResponse(expected);
            std::vector<sai_status_t> objectStatuses(GetParam(), SAI_STATUS_SUCCESS);

            EXPECT_EQ(
                    aggregateStatus,
                    remote->waitForBulkResponse(api, GetParam(), objectStatuses.data()));
            EXPECT_EQ(expected, objectStatuses);
        }
    }
}

TEST_P(BulkResponseTest, AsyncBehaviorRemainsUnchanged)
{
    remote->m_syncMode = false;
    channel->status = SAI_STATUS_FAILURE;
    channel->values.clear();

    for (auto api : apis)
    {
        std::vector<sai_status_t> objectStatuses(GetParam(), SAI_STATUS_FAILURE);
        EXPECT_EQ(
                SAI_STATUS_SUCCESS,
                remote->waitForBulkResponse(api, GetParam(), objectStatuses.data()));
        EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_SUCCESS), objectStatuses);
    }

    EXPECT_EQ(0u, channel->waitCalls);
}

INSTANTIATE_TEST_SUITE_P(OneAndManyObjects, BulkResponseTest, ::testing::Values(1u, 3u));
}
