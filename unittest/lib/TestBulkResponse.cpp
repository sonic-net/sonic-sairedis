#include "Channel.h"
#include "ClientSai.h"
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
        if (throwOnWait)
        {
            throw std::runtime_error("mock wait exception");
        }
        kfvFieldsValues(kco) = values;
        return status;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    std::vector<FieldValueTuple> values;
    bool throwOnWait = false;
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
        client.m_communicationChannel = channel;

        attrCounts.assign(GetParam(), 1);
        attrs.resize(GetParam());
        for (auto& attr : attrs)
        {
            attr.id = SAI_PORT_ATTR_ADMIN_STATE;
            attr.value.booldata = true;
            attrLists.push_back(&attr);
        }
    }

    void setResponse(const std::vector<sai_status_t>& statuses, bool withOids = false)
    {
        channel->values.clear();
        for (auto status : statuses)
        {
            channel->values.emplace_back(sai_serialize_status(status), "");
        }
        if (withOids)
        {
            for (size_t i = 0; i < statuses.size(); ++i)
            {
                channel->values.emplace_back("oid", sai_serialize_object_id(0x100 + i));
            }
        }
    }

    sai_status_t waitForGet(std::vector<sai_status_t>& statuses)
    {
        SWSS_LOG_ENTER();
        return remote->waitForBulkGetResponse(SAI_OBJECT_TYPE_PORT, GetParam(),
                attrCounts.data(), attrLists.data(), statuses.data());
    }

    const std::vector<sai_common_api_t> apis = {
        SAI_COMMON_API_BULK_CREATE, SAI_COMMON_API_BULK_SET, SAI_COMMON_API_BULK_REMOVE
    };
    const std::array<sai_status_t, 2> nonSuccessStatuses = {{
        static_cast<sai_status_t>(SAI_STATUS_FAILURE),
        static_cast<sai_status_t>(SAI_STATUS_UNINITIALIZED),
    }};
    const std::array<sai_status_t, 2> completeStatuses = {{
        static_cast<sai_status_t>(SAI_STATUS_SUCCESS),
        static_cast<sai_status_t>(SAI_STATUS_FAILURE),
    }};
    std::shared_ptr<BulkResponseChannel> channel;
    std::unique_ptr<RedisRemoteSaiInterface> remote;
    ClientSai client;
    std::vector<uint32_t> attrCounts;
    std::vector<sai_attribute_t> attrs;
    std::vector<sai_attribute_t*> attrLists;
};

TEST_P(BulkResponseTest, RemoteNonSuccessWithoutFields)
{
    for (auto api : apis)
    {
        for (auto status : nonSuccessStatuses)
        {
            SCOPED_TRACE(sai_serialize_common_api(api));
            channel->status = status;
            std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
            EXPECT_EQ(status, remote->waitForBulkResponse(api, GetParam(), statuses.data()));
            EXPECT_EQ(std::vector<sai_status_t>(GetParam(), status), statuses);
        }
    }
}

TEST_P(BulkResponseTest, RemoteMalformedResponseRejected)
{
    for (auto api : apis)
    {
        channel->status = SAI_STATUS_SUCCESS;
        channel->values.clear();
        std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
        EXPECT_THROW(remote->waitForBulkResponse(api, GetParam(), statuses.data()), std::runtime_error);
        EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_FAILURE), statuses);

        for (auto responseCount : {GetParam() - 1, GetParam() + 1})
        {
            if (responseCount == 0)
            {
                continue;
            }
            setResponse(std::vector<sai_status_t>(responseCount, SAI_STATUS_SUCCESS));
            for (auto status : completeStatuses)
            {
                channel->status = status;
                EXPECT_THROW(remote->waitForBulkResponse(api, GetParam(), statuses.data()), std::runtime_error);
            }
        }
    }
}

TEST_P(BulkResponseTest, RemoteCompleteResponsePreserved)
{
    for (auto api : apis)
    {
        for (auto status : completeStatuses)
        {
            channel->status = status;
            std::vector<sai_status_t> expected(GetParam(),
                    status == SAI_STATUS_SUCCESS ? SAI_STATUS_SUCCESS : SAI_STATUS_INVALID_PARAMETER);
            if (status != SAI_STATUS_SUCCESS && GetParam() > 1)
            {
                expected.front() = SAI_STATUS_SUCCESS;
                expected.back() = SAI_STATUS_NOT_EXECUTED;
            }
            setResponse(expected);
            std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
            EXPECT_EQ(status, remote->waitForBulkResponse(api, GetParam(), statuses.data()));
            EXPECT_EQ(expected, statuses);
        }
    }
}

TEST_P(BulkResponseTest, RemoteWaitExceptionInitializesStatuses)
{
    channel->throwOnWait = true;
    for (auto api : apis)
    {
        std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
        EXPECT_THROW(remote->waitForBulkResponse(api, GetParam(), statuses.data()), std::runtime_error);
        EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_FAILURE), statuses);
    }
}

TEST_P(BulkResponseTest, RemoteAsyncBehaviorUnchanged)
{
    remote->m_syncMode = false;
    channel->throwOnWait = true;
    for (auto api : apis)
    {
        std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_FAILURE);
        EXPECT_EQ(SAI_STATUS_SUCCESS, remote->waitForBulkResponse(api, GetParam(), statuses.data()));
        EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_SUCCESS), statuses);
    }
    EXPECT_EQ(0u, channel->waitCalls);
}

TEST_P(BulkResponseTest, GetNonSuccessWithoutFieldsLeavesAttributesUntouched)
{
    for (auto status : nonSuccessStatuses)
    {
        channel->status = status;
        std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
        EXPECT_EQ(status, waitForGet(statuses));
        EXPECT_EQ(std::vector<sai_status_t>(GetParam(), status), statuses);
        for (auto& attr : attrs)
        {
            EXPECT_TRUE(attr.value.booldata);
        }
        EXPECT_EQ(std::vector<uint32_t>(GetParam(), 1), attrCounts);
    }
}

TEST_P(BulkResponseTest, GetMalformedResponseRejected)
{
    std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
    EXPECT_THROW(waitForGet(statuses), std::runtime_error);
    EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_FAILURE), statuses);
    for (auto responseCount : {GetParam() - 1, GetParam() + 1})
    {
        if (responseCount == 0)
        {
            continue;
        }
        setResponse(std::vector<sai_status_t>(responseCount, SAI_STATUS_SUCCESS));
        for (auto status : completeStatuses)
        {
            channel->status = status;
            EXPECT_THROW(waitForGet(statuses), std::runtime_error);
        }
    }
}

TEST_P(BulkResponseTest, GetCompleteFailurePreservesPerObjectResults)
{
    channel->status = SAI_STATUS_FAILURE;
    std::vector<sai_status_t> expected(GetParam(), SAI_STATUS_INVALID_PARAMETER);
    if (GetParam() > 1)
    {
        expected.front() = SAI_STATUS_SUCCESS;
    }
    for (auto status : expected)
    {
        channel->values.emplace_back(sai_serialize_status(status),
                status == SAI_STATUS_SUCCESS ? "SAI_PORT_ATTR_ADMIN_STATE=false" : "");
    }
    std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
    EXPECT_EQ(SAI_STATUS_FAILURE, waitForGet(statuses));
    EXPECT_EQ(expected, statuses);
    for (size_t i = 0; i < attrs.size(); ++i)
    {
        EXPECT_EQ(expected[i] != SAI_STATUS_SUCCESS, attrs[i].value.booldata);
    }
}

TEST_P(BulkResponseTest, GetWaitExceptionInitializesStatuses)
{
    channel->throwOnWait = true;
    std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
    EXPECT_THROW(waitForGet(statuses), std::runtime_error);
    EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_FAILURE), statuses);
}

TEST_P(BulkResponseTest, GetBufferOverflowStillTransfersOnlyListCount)
{
    channel->status = SAI_STATUS_FAILURE;
    std::vector<uint32_t> buffers(GetParam(), 0x123);
    for (size_t i = 0; i < attrs.size(); ++i)
    {
        attrs[i].id = SAI_PORT_ATTR_HW_LANE_LIST;
        attrs[i].value.u32list.count = 1;
        attrs[i].value.u32list.list = &buffers[i];
        channel->values.emplace_back(sai_serialize_status(SAI_STATUS_BUFFER_OVERFLOW),
                "SAI_PORT_ATTR_HW_LANE_LIST=2");
    }
    std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
    EXPECT_EQ(SAI_STATUS_FAILURE, waitForGet(statuses));
    EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_BUFFER_OVERFLOW), statuses);
    for (size_t i = 0; i < attrs.size(); ++i)
    {
        EXPECT_EQ(2u, attrs[i].value.u32list.count);
        EXPECT_EQ(&buffers[i], attrs[i].value.u32list.list);
        EXPECT_EQ(0x123u, buffers[i]);
    }
}

TEST_P(BulkResponseTest, ClientNonSuccessWithoutFieldsDropsPreviousCreateOids)
{
    for (auto api : apis)
    {
        for (auto status : nonSuccessStatuses)
        {
            channel->status = status;
            client.m_lastCreateOids.assign(GetParam(), 0x123);
            std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
            EXPECT_EQ(status, client.waitForBulkResponse(api, GetParam(), statuses.data()));
            EXPECT_EQ(std::vector<sai_status_t>(GetParam(), status), statuses);
            if (api == SAI_COMMON_API_BULK_CREATE)
            {
                EXPECT_TRUE(client.m_lastCreateOids.empty());
            }
        }
    }
}

TEST_P(BulkResponseTest, ClientMalformedResponseRejected)
{
    for (auto api : apis)
    {
        channel->status = SAI_STATUS_SUCCESS;
        channel->values.clear();
        std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
        EXPECT_THROW(client.waitForBulkResponse(api, GetParam(), statuses.data()), std::runtime_error);
        EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_FAILURE), statuses);

        setResponse(std::vector<sai_status_t>(GetParam(), SAI_STATUS_SUCCESS),
                api != SAI_COMMON_API_BULK_CREATE);
        for (auto status : completeStatuses)
        {
            channel->status = status;
            EXPECT_THROW(client.waitForBulkResponse(api, GetParam(), statuses.data()), std::runtime_error);
        }
    }
}

TEST_P(BulkResponseTest, ClientCompleteResponsePreserved)
{
    for (auto api : apis)
    {
        for (auto status : completeStatuses)
        {
            channel->status = status;
            std::vector<sai_status_t> expected(GetParam(),
                    status == SAI_STATUS_SUCCESS ? SAI_STATUS_SUCCESS : SAI_STATUS_INVALID_PARAMETER);
            if (status != SAI_STATUS_SUCCESS && GetParam() > 1)
            {
                expected.front() = SAI_STATUS_SUCCESS;
            }
            setResponse(expected, api == SAI_COMMON_API_BULK_CREATE);
            std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
            EXPECT_EQ(status, client.waitForBulkResponse(api, GetParam(), statuses.data()));
            EXPECT_EQ(expected, statuses);
            if (api == SAI_COMMON_API_BULK_CREATE)
            {
                ASSERT_EQ(GetParam(), client.m_lastCreateOids.size());
                for (size_t i = 0; i < expected.size(); ++i)
                {
                    EXPECT_EQ(0x100 + i, client.m_lastCreateOids[i]);
                }
            }
        }
    }
}

TEST_P(BulkResponseTest, ClientWaitExceptionInitializesStatusesAndDropsCreateOids)
{
    channel->throwOnWait = true;
    client.m_lastCreateOids.assign(GetParam(), 0x123);
    std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_SUCCESS);
    EXPECT_THROW(client.waitForBulkResponse(SAI_COMMON_API_BULK_CREATE, GetParam(),
                statuses.data()), std::runtime_error);
    EXPECT_EQ(std::vector<sai_status_t>(GetParam(), SAI_STATUS_FAILURE), statuses);
    EXPECT_TRUE(client.m_lastCreateOids.empty());
}

TEST_P(BulkResponseTest, ClientMalformedCreateOidRejected)
{
    setResponse(std::vector<sai_status_t>(GetParam(), SAI_STATUS_SUCCESS), true);
    channel->values[GetParam()].first = "not_oid";
    std::vector<sai_status_t> statuses(GetParam(), SAI_STATUS_FAILURE);
    EXPECT_THROW(client.waitForBulkResponse(SAI_COMMON_API_BULK_CREATE, GetParam(),
                statuses.data()), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(OneAndManyObjects, BulkResponseTest, ::testing::Values(1u, 3u));
}
