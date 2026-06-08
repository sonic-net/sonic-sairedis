#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "PortLinkEventDamper.h"
#include "MockNotificationHandler.h"

namespace syncd
{

namespace
{

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrictMock;

constexpr sai_object_id_t kVid = 0x10002;
constexpr sai_object_id_t kRid = 0x20001;
constexpr uint32_t kDefaultMaxSuppressTimeMsec = 4500;
constexpr uint32_t kDefaultDecayHalfLifeMsec = 3000;
constexpr uint32_t kDefaultReuseThreshold = 1200;
constexpr uint32_t kDefaultSuppressThreshold = 1500;
constexpr uint32_t kDefaultFlapPenalty = 1000;

constexpr uint32_t kExpectedPenaltyCeiling = 3394;

class TestablePortLinkEventDamper : public PortLinkEventDamper
{
    public:

        TestablePortLinkEventDamper(
                _In_ std::shared_ptr<NotificationHandler> notificationHandler,
                _In_ std::shared_ptr<swss::Select> sel,
                _In_ sai_object_id_t vid,
                _In_ sai_object_id_t rid,
                _In_ const sai_redis_link_event_damping_algo_aied_config_t &config,
                _In_ bool monitorOnly = false)
            : PortLinkEventDamper(notificationHandler, sel, vid, rid, config, monitorOnly),
              m_fakeTimeUsec(1000000)
        {
        }

        uint64_t getCurrentTimeUsecs() const override
        {
            return m_fakeTimeUsec;
        }

        void advanceTimeUsec(uint64_t usec)
        {
            m_fakeTimeUsec += usec;
        }

        void setFakeTimeUsec(uint64_t usec)
        {
            m_fakeTimeUsec = usec;
        }

        uint64_t getFakeTimeUsec() const
        {
            return m_fakeTimeUsec;
        }

    private:

        mutable uint64_t m_fakeTimeUsec;
};

::testing::Matcher<sai_port_oper_status_notification_t> EqOperStatus(
        _In_ const sai_port_oper_status_notification_t &expected)
{
    SWSS_LOG_ENTER();

    return AllOf(
            Field("port_id", &sai_port_oper_status_notification_t::port_id,
                  expected.port_id),
            Field("port_state", &sai_port_oper_status_notification_t::port_state,
                  expected.port_state));
}

class PortLinkEventDamperTest : public ::testing::Test
{
    protected:

        PortLinkEventDamperTest()
            : m_portLinkEventDamper(nullptr),
              m_notificationHandler(nullptr) {}

        ~PortLinkEventDamperTest() override {}

        void SetUp() override;

        void TearDown() override
        {
            if (m_portLinkEventDamper != nullptr)
            {
                delete m_portLinkEventDamper;
            }
        }

        const sai_redis_link_event_damping_algo_aied_config_t kDefaultConfig = {
                .max_suppress_time = kDefaultMaxSuppressTimeMsec,
                .suppress_threshold = kDefaultSuppressThreshold,
                .reuse_threshold = kDefaultReuseThreshold,
                .decay_half_life = kDefaultDecayHalfLifeMsec,
                .flap_penalty = kDefaultFlapPenalty};

        TestablePortLinkEventDamper *m_portLinkEventDamper;
        std::shared_ptr<StrictMock<MockNotificationHandler>> m_notificationHandler;
};

void PortLinkEventDamperTest::SetUp()
{
    SWSS_LOG_ENTER();

    std::shared_ptr<NotificationProcessor> notificationProcessor =
            std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr);
    m_notificationHandler = std::make_shared<StrictMock<MockNotificationHandler>>(
            notificationProcessor);

    m_portLinkEventDamper =
            new TestablePortLinkEventDamper(m_notificationHandler,
                                           std::make_shared<swss::Select>(),
                                           kVid, kRid, kDefaultConfig);
}

void activateDampingOnPort(
        _In_ TestablePortLinkEventDamper *damper,
        _In_ std::shared_ptr<StrictMock<MockNotificationHandler>> notificationHandler)
{
    SWSS_LOG_ENTER();

    constexpr sai_port_oper_status_t kPortStateUp = SAI_PORT_OPER_STATUS_UP;
    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;

    while (!damper->isActive())
    {
        sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                    .port_state = kPortStateUp};
        EXPECT_CALL(*notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        damper->handlePortStateChange(kPortStateUp);

        data.port_state = kPortStateDown;
        EXPECT_CALL(*notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        damper->handlePortStateChange(kPortStateDown);

        damper->advanceTimeUsec(10);
    }

    EXPECT_TRUE(damper->isActive());
    EXPECT_GE(damper->getAccumulatedPenalty(), kDefaultSuppressThreshold);
}

void setUpLinkEventDamperDestruction(
        _In_ TestablePortLinkEventDamper *damper,
        _In_ std::shared_ptr<StrictMock<MockNotificationHandler>> notificationHandler)
{
    SWSS_LOG_ENTER();

    sai_port_oper_status_notification_t data = {
            .port_id = kRid, .port_state = damper->getActualState()};

    if (damper->getAdvertisedState() != damper->getActualState())
    {
        EXPECT_CALL(*notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());
    }
    else
    {
        EXPECT_CALL(*notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .Times(0);
    }
}

TEST_F(PortLinkEventDamperTest, LinkEventDampingSetupSucceeds)
{
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_EQ(m_portLinkEventDamper->getActualState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_FALSE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_FALSE(m_portLinkEventDamper->isActive());

    m_portLinkEventDamper->setup();

    EXPECT_EQ(m_portLinkEventDamper->getPenaltyCeiling(), kExpectedPenaltyCeiling);
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, VerifyUpdatePenaltyCeiling)
{
    m_portLinkEventDamper->setup();

    struct Data {
        sai_redis_link_event_damping_algo_aied_config_t config;
        uint32_t expected_penalty_ceiling;
    };

    const std::vector<Data> kTestData = {
            {{.max_suppress_time = 64,
              .suppress_threshold = 100,
              .reuse_threshold = 1500,
              .decay_half_life = 45,
              .flap_penalty = 100}, 4019},
            {{.max_suppress_time = 64,
              .suppress_threshold = 100,
              .reuse_threshold = 50,
              .decay_half_life = 4,
              .flap_penalty = 100}, 3276800},
            {{.max_suppress_time = 35,
              .suppress_threshold = 100,
              .reuse_threshold = 132,
              .decay_half_life = 7,
              .flap_penalty = 100}, 4224},
            {{.max_suppress_time = 45,
              .suppress_threshold = 100,
              .reuse_threshold = 700,
              .decay_half_life = 9,
              .flap_penalty = 100}, 22400},
    };

    for (const auto &data : kTestData)
    {
        SCOPED_TRACE(::testing::Message()
                     << "Max suppress time: " << data.config.max_suppress_time
                     << ", half life(msec): " << data.config.decay_half_life
                     << ", reuse threshold: " << data.config.reuse_threshold);

        m_portLinkEventDamper->updateLinkEventDampingConfig(data.config);
        EXPECT_EQ(m_portLinkEventDamper->getPenaltyCeiling(),
                  data.expected_penalty_ceiling);
    }

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, DampingTimerExpirationDoesNotDisableDamping)
{
    m_portLinkEventDamper->setup();

    activateDampingOnPort(m_portLinkEventDamper, m_notificationHandler);

    DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    m_portLinkEventDamper->advanceTimeUsec(
            kDefaultDecayHalfLifeMsec * 1000ULL / 2);

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);

    m_portLinkEventDamper->handleSelectableEvent();

    EXPECT_TRUE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_GT(m_portLinkEventDamper->getAccumulatedPenalty(),
              kDefaultReuseThreshold);

    DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(currDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest,
       DampingTimerExpirationDisablesDampingOnPortAndAdvertiseState)
{
    m_portLinkEventDamper->setup();

    activateDampingOnPort(m_portLinkEventDamper, m_notificationHandler);

    DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    {
        sai_port_oper_status_notification_t data = {
                .port_id = kRid, .port_state = SAI_PORT_OPER_STATUS_UP};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .Times(0);

        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);
    }

    m_portLinkEventDamper->advanceTimeUsec(
            kDefaultDecayHalfLifeMsec * 1000ULL * 3);

    sai_port_oper_status_notification_t data = {
            .port_id = kRid, .port_state = SAI_PORT_OPER_STATUS_UP};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
            .WillOnce(Return());

    m_portLinkEventDamper->handleSelectableEvent();

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_LE(m_portLinkEventDamper->getAccumulatedPenalty(),
              kDefaultReuseThreshold);
    EXPECT_EQ(m_portLinkEventDamper->getActualState(), SAI_PORT_OPER_STATUS_UP);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), SAI_PORT_OPER_STATUS_UP);

    DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + 1);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest,
       DampingTimerExpirationDisablesDampingOnPortAndDoesNotAdvertiseState)
{
    m_portLinkEventDamper->setup();

    activateDampingOnPort(m_portLinkEventDamper, m_notificationHandler);

    m_portLinkEventDamper->advanceTimeUsec(
            kDefaultDecayHalfLifeMsec * 1000ULL * 3);

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);

    m_portLinkEventDamper->handleSelectableEvent();

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_EQ(m_portLinkEventDamper->getActualState(), SAI_PORT_OPER_STATUS_DOWN);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(),
              SAI_PORT_OPER_STATUS_DOWN);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest,
       VerifyPortEventsAdvertisedImmediatelyOnConfigDisabledPort)
{
    constexpr sai_port_oper_status_t kPortStateUp = SAI_PORT_OPER_STATUS_UP;
    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;
    DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    constexpr int kPortFlapCount = 10;
    for (int idx = 0; idx < kPortFlapCount; ++idx)
    {
        SCOPED_TRACE(::testing::Message() << "Testing iteration: " << idx);
        sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                    .port_state = kPortStateUp};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        m_portLinkEventDamper->handlePortStateChange(kPortStateUp);
        EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), kPortStateUp);

        data.port_state = kPortStateDown;
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        m_portLinkEventDamper->handlePortStateChange(kPortStateDown);
        EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), kPortStateDown);
    }

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_FALSE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_EQ(m_portLinkEventDamper->getAccumulatedPenalty(), 0);

    DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + kPortFlapCount);
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + kPortFlapCount);
    EXPECT_EQ(currDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + kPortFlapCount);
    EXPECT_EQ(currDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events + kPortFlapCount);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, UpEventWhileDampingNotActiveAdvertisesUpEvent)
{
    m_portLinkEventDamper->setup();

    sai_port_oper_status_notification_t dataDown = {.port_id = kRid,
                                                    .port_state = SAI_PORT_OPER_STATUS_DOWN};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataDown))))
            .WillOnce(Return());
    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_DOWN);

    uint32_t prev_penalty = m_portLinkEventDamper->getAccumulatedPenalty();

    sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                  .port_state = SAI_PORT_OPER_STATUS_UP};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
            .WillOnce(Return());

    DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();
    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_EQ(m_portLinkEventDamper->getAccumulatedPenalty(), prev_penalty);

    DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + 1);
    EXPECT_EQ(m_portLinkEventDamper->getActualState(), SAI_PORT_OPER_STATUS_UP);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), SAI_PORT_OPER_STATUS_UP);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest,
       DownEventWhileDampingNotActiveDoesNotStartDamping)
{
    m_portLinkEventDamper->setup();

    sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                  .port_state = SAI_PORT_OPER_STATUS_UP};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
            .WillOnce(Return());
    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

    sai_port_oper_status_notification_t dataDown = {.port_id = kRid,
                                                    .port_state = SAI_PORT_OPER_STATUS_DOWN};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataDown))))
            .WillOnce(Return());

    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_DOWN);

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_LT(m_portLinkEventDamper->getAccumulatedPenalty(),
              kDefaultSuppressThreshold);
    EXPECT_EQ(m_portLinkEventDamper->getActualState(), SAI_PORT_OPER_STATUS_DOWN);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), SAI_PORT_OPER_STATUS_DOWN);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, DownEventWhileDampingNotActiveStartsDamping)
{
    m_portLinkEventDamper->setup();

    sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                  .port_state = SAI_PORT_OPER_STATUS_UP};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
            .WillOnce(Return());
    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

    for (int i = 0; i < 2; ++i)
    {
        sai_port_oper_status_notification_t dataDown = {.port_id = kRid,
                                                        .port_state = SAI_PORT_OPER_STATUS_DOWN};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataDown))))
                .WillOnce(Return());
        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_DOWN);

        if (m_portLinkEventDamper->isActive())
            break;

        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
                .WillOnce(Return());
        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);
    }

    EXPECT_TRUE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_GE(m_portLinkEventDamper->getAccumulatedPenalty(),
              kDefaultSuppressThreshold);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, UpEventWhileDampingActiveIsDamped)
{
    m_portLinkEventDamper->setup();

    activateDampingOnPort(m_portLinkEventDamper, m_notificationHandler);

    DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    uint32_t prev_penalty = m_portLinkEventDamper->getAccumulatedPenalty();

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);

    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_TRUE(m_portLinkEventDamper->isActive());
    EXPECT_EQ(m_portLinkEventDamper->getAccumulatedPenalty(), prev_penalty);

    DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(m_portLinkEventDamper->getActualState(), SAI_PORT_OPER_STATUS_UP);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(),
              SAI_PORT_OPER_STATUS_DOWN);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, DownEventWhileDampingActiveIsDamped)
{
    m_portLinkEventDamper->setup();

    activateDampingOnPort(m_portLinkEventDamper, m_notificationHandler);

    sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                  .port_state = SAI_PORT_OPER_STATUS_UP};
    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);
    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

    DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);

    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_DOWN);

    EXPECT_TRUE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_GT(m_portLinkEventDamper->getAccumulatedPenalty(),
              kDefaultReuseThreshold);

    DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);
    EXPECT_EQ(m_portLinkEventDamper->getActualState(), SAI_PORT_OPER_STATUS_DOWN);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), SAI_PORT_OPER_STATUS_UP);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, UpdatingSameLinkEventDampingConfigIsNoOp)
{
    m_portLinkEventDamper->updateLinkEventDampingConfig(kDefaultConfig);

    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_FALSE(m_portLinkEventDamper->isActive());

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, ConfigUpdateClearsActiveDamping)
{
    m_portLinkEventDamper->setup();

    activateDampingOnPort(m_portLinkEventDamper, m_notificationHandler);
    EXPECT_TRUE(m_portLinkEventDamper->isActive());

    sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                  .port_state = SAI_PORT_OPER_STATUS_UP};
    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);
    m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
            .WillOnce(Return());

    sai_redis_link_event_damping_algo_aied_config_t newConfig = kDefaultConfig;
    newConfig.reuse_threshold = 1100;
    m_portLinkEventDamper->updateLinkEventDampingConfig(newConfig);

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());
    EXPECT_EQ(m_portLinkEventDamper->getAccumulatedPenalty(), 0);
    EXPECT_EQ(m_portLinkEventDamper->getAdvertisedState(), SAI_PORT_OPER_STATUS_UP);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

class PortLinkEventDamperDisabledConfigTest
    : public PortLinkEventDamperTest,
      public testing::WithParamInterface<
          sai_redis_link_event_damping_algo_aied_config_t> {};

std::string PortLinkEventDamperDisabledConfigTestCaseName(
    _In_ const testing::TestParamInfo<
        sai_redis_link_event_damping_algo_aied_config_t> &info)
{
    SWSS_LOG_ENTER();

    std::stringstream ss;
    if (info.param.max_suppress_time == 0) {
        ss << "MaxSuppresssTimeIsZero";
    }

    if (info.param.suppress_threshold == 0) {
        ss << "SuppressThresholdIsZero";
    }

    if (info.param.reuse_threshold == 0) {
        ss << "ReuseThresholdIsZero";
    }

    if (info.param.decay_half_life == 0) {
        ss << "DecayHalfLifeIsZero";
    }

    if (info.param.flap_penalty == 0) {
        ss << "FlapPenaltyIsZero";
    }

    return ss.str();
}

TEST_P(PortLinkEventDamperDisabledConfigTest,
       LinkEventDampingSetupSucceedsWithDisabledConfig)
{
    sai_redis_link_event_damping_algo_aied_config_t param = GetParam();

    m_portLinkEventDamper->updateLinkEventDampingConfig(param);

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(1, _))
            .Times(0);

    m_portLinkEventDamper->setup();

    EXPECT_EQ(m_portLinkEventDamper->getPenaltyCeiling(), 0);
    EXPECT_FALSE(m_portLinkEventDamper->isConfigEnabled());

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

INSTANTIATE_TEST_SUITE_P(PortLinkEventDamperTestInstantiation,
                         PortLinkEventDamperDisabledConfigTest,
                         ::testing::Values(
                             (sai_redis_link_event_damping_algo_aied_config_t){
                                 .max_suppress_time = 0,
                                 .suppress_threshold = 100,
                                 .reuse_threshold = 100,
                                 .decay_half_life = 100,
                                 .flap_penalty = 100},
                             (sai_redis_link_event_damping_algo_aied_config_t){
                                 .max_suppress_time = 100,
                                 .suppress_threshold = 0,
                                 .reuse_threshold = 100,
                                 .decay_half_life = 100,
                                 .flap_penalty = 100},
                             (sai_redis_link_event_damping_algo_aied_config_t){
                                 .max_suppress_time = 100,
                                 .suppress_threshold = 100,
                                 .reuse_threshold = 0,
                                 .decay_half_life = 100,
                                 .flap_penalty = 100},
                             (sai_redis_link_event_damping_algo_aied_config_t){
                                 .max_suppress_time = 100,
                                 .suppress_threshold = 100,
                                 .reuse_threshold = 100,
                                 .decay_half_life = 0,
                                 .flap_penalty = 100},
                             (sai_redis_link_event_damping_algo_aied_config_t){
                                 .max_suppress_time = 100,
                                 .suppress_threshold = 100,
                                 .reuse_threshold = 100,
                                 .decay_half_life = 100,
                                 .flap_penalty = 0}),
                         PortLinkEventDamperDisabledConfigTestCaseName);

TEST_F(PortLinkEventDamperTest, MonitorOnlyModeForwardsAllEvents)
{
    delete m_portLinkEventDamper;

    std::shared_ptr<NotificationProcessor> notificationProcessor =
            std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr);
    m_notificationHandler = std::make_shared<StrictMock<MockNotificationHandler>>(
            notificationProcessor);

    m_portLinkEventDamper =
            new TestablePortLinkEventDamper(m_notificationHandler,
                                           std::make_shared<swss::Select>(),
                                           kVid, kRid, kDefaultConfig,
                                           true);
    m_portLinkEventDamper->setup();

    constexpr int kPortFlapCount = 10;
    for (int idx = 0; idx < kPortFlapCount; ++idx)
    {
        sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                      .port_state = SAI_PORT_OPER_STATUS_UP};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
                .WillOnce(Return());
        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

        sai_port_oper_status_notification_t dataDown = {.port_id = kRid,
                                                        .port_state = SAI_PORT_OPER_STATUS_DOWN};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataDown))))
                .WillOnce(Return());
        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    }

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_TRUE(m_portLinkEventDamper->isConfigEnabled());

    DampingStats stats = m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(stats.pre_damping_up_events, kPortFlapCount);
    EXPECT_EQ(stats.pre_damping_down_events, kPortFlapCount);
    EXPECT_EQ(stats.post_damping_up_events, kPortFlapCount);
    EXPECT_EQ(stats.post_damping_down_events, kPortFlapCount);

    EXPECT_GT(m_portLinkEventDamper->getAccumulatedPenalty(), 0u);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, MonitorOnlyModeNeverActivatesDamping)
{
    delete m_portLinkEventDamper;

    std::shared_ptr<NotificationProcessor> notificationProcessor =
            std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr);
    m_notificationHandler = std::make_shared<StrictMock<MockNotificationHandler>>(
            notificationProcessor);

    m_portLinkEventDamper =
            new TestablePortLinkEventDamper(m_notificationHandler,
                                           std::make_shared<swss::Select>(),
                                           kVid, kRid, kDefaultConfig,
                                           true);
    m_portLinkEventDamper->setup();

    for (int i = 0; i < 20; ++i)
    {
        sai_port_oper_status_notification_t dataUp = {.port_id = kRid,
                                                      .port_state = SAI_PORT_OPER_STATUS_UP};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataUp))))
                .WillOnce(Return());
        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_UP);

        sai_port_oper_status_notification_t dataDown = {.port_id = kRid,
                                                        .port_state = SAI_PORT_OPER_STATUS_DOWN};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(1, Pointee(EqOperStatus(dataDown))))
                .WillOnce(Return());
        m_portLinkEventDamper->handlePortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    }

    EXPECT_FALSE(m_portLinkEventDamper->isActive());
    EXPECT_GE(m_portLinkEventDamper->getAccumulatedPenalty(),
              kDefaultSuppressThreshold);

    setUpLinkEventDamperDestruction(m_portLinkEventDamper, m_notificationHandler);
}

}  // namespace
}  // namespace syncd
