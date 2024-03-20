#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "PortLinkEventDamper.h"
#include "MockNotificationHandler.h"

namespace syncd
{

// Peer class to access private state and APIs.
class PortLinkEventDamperPeer
{
    public:
        explicit PortLinkEventDamperPeer(
                _In_ PortLinkEventDamper *portLinkEventDamper)
            : m_portLinkEventDamper(portLinkEventDamper) {}

        void setTimerActive(
                _In_ bool state)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_timerActive = state;
        }

        bool getTimerActive() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_timerActive;
        }

        void setAdvertisedState(
                _In_ sai_port_oper_status_t portState)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_advertisedState = portState;
        }

        void setActualState(
                _In_ sai_port_oper_status_t portState)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_actualState = portState;
        }

        void setMaxSuppressTime(
                _In_ uint64_t time)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_maxSuppressTimeUsec = time;
        }

        void setDecayHalfLife(
                _In_ uint64_t time)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_decayHalfLifeUsec = time;
        }

        void setReuseThreshold(
                _In_ uint32_t threshold)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_reuseThreshold = threshold;
        }

        void setSuppressThreshold(
                _In_ uint32_t threshold)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_suppressThreshold = threshold;
        }

        void setFlapPenalty(
                _In_ uint32_t val)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_flapPenalty = val;
        }

        void setAccumulatedPenalty(
                _In_ uint32_t penalty)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_accumulatedPenalty = penalty;
        }

        void setLastPenaltyUpdateTimestamp(
                _In_ uint64_t timestamp)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_lastPenaltyUpdateTimestamp = timestamp;
        }

        void setDampingConfigEnabled(
                _In_ bool state)
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->m_dampingConfigEnabled = state;
        }

        sai_port_oper_status_t getAdvertisedState() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_advertisedState;
        }

        sai_port_oper_status_t getActualState() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_actualState;
        }

        uint64_t getMaxSuppressTime() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_maxSuppressTimeUsec;
        }

        uint64_t getDecayHalfLife() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_decayHalfLifeUsec;
        }

        uint32_t getSuppressThreshold() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_suppressThreshold;
        }

        uint32_t getReuseThreshold() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_reuseThreshold;
        }

        uint32_t getFlapPenalty() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_flapPenalty;
        }

        bool getDampingConfigEnabled() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_dampingConfigEnabled;
        }

        bool getDampingActive() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_dampingActive;
        }

        uint32_t getAccumulatedPenalty() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_accumulatedPenalty;
        }

        uint32_t getPenaltyCeiling() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_penaltyCeiling;
        }

        bool getTimerAddedToSelect() const
        {
            SWSS_LOG_ENTER();

            return m_portLinkEventDamper->m_timerAddedToSelect;
        }

        void updatePenaltyCeiling()
        {
            SWSS_LOG_ENTER();

            m_portLinkEventDamper->updatePenaltyCeiling();
        }

    private:
        PortLinkEventDamper *m_portLinkEventDamper;  // Not owned.
};

namespace
{

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

constexpr sai_object_id_t kVid = 0x10002;
constexpr sai_object_id_t kRid = 0x20001;
constexpr uint32_t kDefaultMaxSuppressTimeMsec = 4500;
constexpr uint32_t kDefaultDecayHalfLifeMsec = 3000;
constexpr uint32_t kDefaultReuseThreshold = 1200;
constexpr uint32_t kDefaultSuppressThreshold = 1500;
constexpr uint32_t kDefaultFlapPenalty = 1000;

constexpr uint32_t kExpectedPenaltyCeiling = 3394;

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

        PortLinkEventDamper *m_portLinkEventDamper;
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
            new PortLinkEventDamper(m_notificationHandler,
                                    std::make_shared<swss::Select>(),
                                    kVid, kRid, kDefaultConfig);
}

void verifyStatesAfterPortDamperCreation(
        _In_ const PortLinkEventDamperPeer &perPortDamperPeer)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_EQ(perPortDamperPeer.getActualState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_EQ(perPortDamperPeer.getMaxSuppressTime(),
              kDefaultMaxSuppressTimeMsec * MICRO_SECS_PER_MILLI_SEC);
    EXPECT_EQ(perPortDamperPeer.getDecayHalfLife(),
              kDefaultDecayHalfLifeMsec * MICRO_SECS_PER_MILLI_SEC);
    EXPECT_EQ(perPortDamperPeer.getSuppressThreshold(),
              kDefaultSuppressThreshold);
    EXPECT_EQ(perPortDamperPeer.getReuseThreshold(), kDefaultReuseThreshold);
    EXPECT_EQ(perPortDamperPeer.getFlapPenalty(), kDefaultFlapPenalty);
    EXPECT_EQ(perPortDamperPeer.getAccumulatedPenalty(), 0);
    EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(),
              std::numeric_limits<uint32_t>::max());
    EXPECT_FALSE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getTimerAddedToSelect());
}

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

// Sets up the expectations for link event damper object destruction.
void setUpLinkEventDamperDestruction(
        _In_ PortLinkEventDamperPeer &perPortDamperPeer,
        _In_ std::shared_ptr<StrictMock<MockNotificationHandler>> notificationHandler)
{
    SWSS_LOG_ENTER();

    sai_port_oper_status_notification_t data = {
            .port_id = kRid, .port_state = perPortDamperPeer.getActualState()};
    if (perPortDamperPeer.getAdvertisedState() != perPortDamperPeer.getActualState())
    {
        // Set expectation to advertise the actual state.
        EXPECT_CALL(*notificationHandler,
                onPortStateChangePostLinkEventDamping(/*count=*/1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());
    }
    else
    {
        // No advertisement of actual state.
        EXPECT_CALL(*notificationHandler,
                onPortStateChangePostLinkEventDamping(/*count=*/1, Pointee(EqOperStatus(data))))
                .Times(0);
    }
}

void verifyStatesAfterLinkEventDampingConfigUpdate(
        _In_ const PortLinkEventDamperPeer &perPortDamperPeer,
        _In_ const sai_redis_link_event_damping_algo_aied_config_t &expectedConfig,
        _In_ sai_port_oper_status_t expectedState,
        _In_ bool dampingConfigEnabled)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), expectedState);
    EXPECT_EQ(perPortDamperPeer.getActualState(), expectedState);
    EXPECT_EQ(perPortDamperPeer.getMaxSuppressTime(),
              expectedConfig.max_suppress_time * MICRO_SECS_PER_MILLI_SEC);
    EXPECT_EQ(perPortDamperPeer.getDecayHalfLife(),
              expectedConfig.decay_half_life * MICRO_SECS_PER_MILLI_SEC);
    EXPECT_EQ(perPortDamperPeer.getSuppressThreshold(),
              expectedConfig.suppress_threshold);
    EXPECT_EQ(perPortDamperPeer.getReuseThreshold(),
              expectedConfig.reuse_threshold);
    EXPECT_EQ(perPortDamperPeer.getFlapPenalty(), expectedConfig.flap_penalty);
    EXPECT_EQ(perPortDamperPeer.getAccumulatedPenalty(), 0);
    EXPECT_EQ(perPortDamperPeer.getDampingConfigEnabled(), dampingConfigEnabled);
    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getTimerAddedToSelect());
}

void activateDampingOnPort(
        _In_ PortLinkEventDamper *perPortDamper,
        _In_ PortLinkEventDamperPeer &perPortDamperPeer,
        _In_ std::shared_ptr<StrictMock<MockNotificationHandler>> notificationHandler)
{
    SWSS_LOG_ENTER();

    uint32_t suppressThreshold = perPortDamperPeer.getSuppressThreshold();
    constexpr sai_port_oper_status_t kPortStateUp = SAI_PORT_OPER_STATUS_UP;
    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;
    struct DampingStats initialDampingStats = perPortDamper->getDampingStats();

    uint32_t upLinkEvents = 0;
    uint32_t downLinkEvents = 0;

    // Generate DOWN events till damping activates on the port.
    while ((perPortDamperPeer.getAccumulatedPenalty() < suppressThreshold) &&
           (!perPortDamperPeer.getDampingActive()))
    {
        sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                    .port_state = kPortStateUp};
        EXPECT_CALL(*notificationHandler,
                    onPortStateChangePostLinkEventDamping(
                        /*count=*/1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        perPortDamper->handlePortStateChange(kPortStateUp);
        ++upLinkEvents;

        data.port_state = kPortStateDown;
        EXPECT_CALL(*notificationHandler,
                    onPortStateChangePostLinkEventDamping(
                        /*count=*/1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        perPortDamper->handlePortStateChange(kPortStateDown);
        ++downLinkEvents;
    }

    EXPECT_TRUE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getTimerActive());
    EXPECT_GE(perPortDamperPeer.getAccumulatedPenalty(), suppressThreshold);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_DOWN);
    EXPECT_EQ(perPortDamperPeer.getActualState(), SAI_PORT_OPER_STATUS_DOWN);

    struct DampingStats finalDampingStats = perPortDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + upLinkEvents);
    EXPECT_EQ(finalDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + downLinkEvents);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + upLinkEvents);
    EXPECT_EQ(finalDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events + downLinkEvents);
}

TEST_F(PortLinkEventDamperTest, LinkEventDampingSetupSucceeds)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    m_portLinkEventDamper->setup();

    EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(), kExpectedPenaltyCeiling);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_EQ(perPortDamperPeer.getActualState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_TRUE(perPortDamperPeer.getTimerAddedToSelect());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

TEST_F(PortLinkEventDamperTest, VerifyUpdatePenaltyCeiling)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    struct Data {
        uint64_t max_suppress_time;
        uint64_t decay_half_life;
        uint32_t reuse_threshold;
        uint32_t expected_penalty_ceiling;
    };

    const std::vector<struct Data> kTestData = {
            {64, 45, 1500, 4019},
            {64, 4, 50, 3276800},
            {64, 2, 1000, std::numeric_limits<uint32_t>::max()},
            {35, 7, 132, 4224},
            {45, 9, 700, 22400},
            /* Calculated penalty is expected to be 0 if any of config param is 0. */
            {0, 10, 20, 0},
            {10, 0, 20, 0},
            {10, 4, 0, 0}};

    for (const auto &data : kTestData)
    {
        SCOPED_TRACE(::testing::Message()
                     << "Max suppress time: " << data.max_suppress_time
                     << ", half life(usec): " << data.decay_half_life
                     << ", reuse threshold: " << data.reuse_threshold
                     << ", expected penalty ceiling: "
                     << data.expected_penalty_ceiling);

        perPortDamperPeer.setMaxSuppressTime(data.max_suppress_time);
        perPortDamperPeer.setDecayHalfLife(data.decay_half_life);
        perPortDamperPeer.setReuseThreshold(data.reuse_threshold);

        perPortDamperPeer.updatePenaltyCeiling();
        EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(),
                  data.expected_penalty_ceiling);
    }

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests that when damping timer is expired and accumulated penalty after decay
// is above reuse threshold, no port state is advertised, damping on the port
// remains active and new damping timer is set.
TEST_F(PortLinkEventDamperTest, DampingTimerExpirationDoesNotDisableDamping)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    activateDampingOnPort(m_portLinkEventDamper, perPortDamperPeer,
                          m_notificationHandler);

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();
    perPortDamperPeer.setAccumulatedPenalty(
            perPortDamperPeer.getReuseThreshold() * 4);

    // Set decay interval to half time duration, so that accumulated penalty
    // after decay still remains above reuse threshold.
    perPortDamperPeer.setLastPenaltyUpdateTimestamp(
            getCurrentTimeUsecs() - perPortDamperPeer.getDecayHalfLife());
    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_DOWN);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_UP);

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(/*count=*/1, _))
            .Times(0);

    // Simulate a timer event.
    m_portLinkEventDamper->handleSelectableEvent();

    EXPECT_TRUE(perPortDamperPeer.getTimerActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_GT(perPortDamperPeer.getAccumulatedPenalty(),
              perPortDamperPeer.getReuseThreshold());

    struct DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(currDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);
    EXPECT_EQ(currDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(currDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_DOWN);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests that when damping timer is expired and accumulated penalty after decay
// is below reuse threshold, damping is disabled on the port. If current port
// operational status is not same as advertised state, advertise it.
TEST_F(PortLinkEventDamperTest,
       DampingTimerExpirationDisablesDampingOnPortAndAdvertiseState)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    activateDampingOnPort(m_portLinkEventDamper, perPortDamperPeer,
                          m_notificationHandler);

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    // Set decay interval to half time duration and current accumulated penalty
    // less than twice of reuse threshold so that accumulated penalty after decay
    // goes below reuse threshold.
    perPortDamperPeer.setAccumulatedPenalty(
            (perPortDamperPeer.getReuseThreshold() * 3) / 2);
    perPortDamperPeer.setLastPenaltyUpdateTimestamp(
            getCurrentTimeUsecs() - perPortDamperPeer.getDecayHalfLife());
    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_DOWN);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_UP);

    sai_port_oper_status_notification_t data = {
            .port_id = kRid, .port_state = perPortDamperPeer.getActualState()};

    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(
                    /*count=*/1, Pointee(EqOperStatus(data))))
            .WillOnce(Return());

    // Simulate a timer event, which should cause onDampedPortstateChange to be
    // called and the actual state to be advertised.
    m_portLinkEventDamper->handleSelectableEvent();

    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_LE(perPortDamperPeer.getAccumulatedPenalty(),
              perPortDamperPeer.getReuseThreshold());
    EXPECT_EQ(perPortDamperPeer.getActualState(), SAI_PORT_OPER_STATUS_UP);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), SAI_PORT_OPER_STATUS_UP);

    struct DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + 1);
    EXPECT_EQ(currDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);
    EXPECT_NE(currDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(currDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests that when damping timer is expired and accumulated penalty after decay
// is below reuse threshold, damping is disabled on the port. If current port
// operational status is same as advertised state, don't advertise it.
TEST_F(PortLinkEventDamperTest,
       DampingTimerExpirationDisablesDampingOnPortAndDoesNotAdvertiseState)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    activateDampingOnPort(m_portLinkEventDamper, perPortDamperPeer,
                          m_notificationHandler);

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    // Set decay interval to 1 half time duration and current accumulated penalty
    // such that accumulated penalty after decay goes below reuse threshold.
    perPortDamperPeer.setAccumulatedPenalty(
            (perPortDamperPeer.getReuseThreshold() * 3) / 2);
    perPortDamperPeer.setLastPenaltyUpdateTimestamp(
            getCurrentTimeUsecs() - perPortDamperPeer.getDecayHalfLife());
    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_DOWN);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_DOWN);

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(/*count=*/1, _))
            .Times(0);
    // Simulate a timer event.
    m_portLinkEventDamper->handleSelectableEvent();

    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_LE(perPortDamperPeer.getAccumulatedPenalty(),
              perPortDamperPeer.getReuseThreshold());
    EXPECT_EQ(perPortDamperPeer.getActualState(), SAI_PORT_OPER_STATUS_DOWN);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_DOWN);

    struct DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(currDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);
    EXPECT_EQ(currDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(currDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests port events on damping config disabled port, get advertised
// immediately.
TEST_F(PortLinkEventDamperTest,
       VerifyPortEventsAdvertisedImmediatelyOnConfigDisabledPort)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    constexpr sai_port_oper_status_t kPortStateUp = SAI_PORT_OPER_STATUS_UP;
    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;
    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    // Generate few port events.
    constexpr int kPortFlapCount = 10;
    for (int idx = 0; idx < kPortFlapCount; ++idx)
    {
        SCOPED_TRACE(::testing::Message() << "Testing iteration: " << idx);
        sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                    .port_state = kPortStateUp};
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(
                        /*count=*/1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        m_portLinkEventDamper->handlePortStateChange(kPortStateUp);
        EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), kPortStateUp);
        EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateUp);

        data.port_state = kPortStateDown;
        EXPECT_CALL(*m_notificationHandler,
                    onPortStateChangePostLinkEventDamping(
                        /*count=*/1, Pointee(EqOperStatus(data))))
                .WillOnce(Return());

        m_portLinkEventDamper->handlePortStateChange(kPortStateDown);
        EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), kPortStateDown);
        EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateDown);
    }

    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_EQ(perPortDamperPeer.getAccumulatedPenalty(), 0);

    struct DampingStats currDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(currDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + kPortFlapCount);
    EXPECT_EQ(currDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + kPortFlapCount);
    EXPECT_EQ(currDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + kPortFlapCount);
    EXPECT_EQ(currDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events + kPortFlapCount);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests when damping is not active on port and an UP event is observed,
// advertise the UP event immediately.
TEST_F(PortLinkEventDamperTest, UpEventWhileDampingNotActiveAdvertisesUpEvent)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    // Set the test scenarios.
    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_DOWN);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_DOWN);

    constexpr sai_port_oper_status_t kPortStateUp = SAI_PORT_OPER_STATUS_UP;
    uint32_t prev_penalty = perPortDamperPeer.getAccumulatedPenalty();

    sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                .port_state = kPortStateUp};
    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(
                    /*count=*/1, Pointee(EqOperStatus(data))))
            .WillOnce(Return());

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();
    m_portLinkEventDamper->handlePortStateChange(kPortStateUp);

    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_EQ(perPortDamperPeer.getAccumulatedPenalty(), prev_penalty);

    struct DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events + 1);
    EXPECT_NE(finalDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(finalDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events);
    EXPECT_EQ(finalDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);
    EXPECT_EQ(finalDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);
    EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateUp);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), kPortStateUp);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests when damping is not active on port and next DOWN event doesn't activate
// damping on port, advertise the DOWN event immediately.
TEST_F(PortLinkEventDamperTest,
       DownEventWhileDampingNotActiveDoesNotStartDamping)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    // Set the test scenarios.
    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_UP);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_UP);
    perPortDamperPeer.setAccumulatedPenalty(0);
    perPortDamperPeer.setLastPenaltyUpdateTimestamp(
            getCurrentTimeUsecs() - perPortDamperPeer.getDecayHalfLife());

    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;
    sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                .port_state = kPortStateDown};

    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(
                    /*count=*/1, Pointee(EqOperStatus(data))))
            .WillOnce(Return());

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();
    m_portLinkEventDamper->handlePortStateChange(kPortStateDown);

    EXPECT_FALSE(perPortDamperPeer.getTimerActive());
    EXPECT_FALSE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_LT(perPortDamperPeer.getAccumulatedPenalty(),
              perPortDamperPeer.getSuppressThreshold());

    struct DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events + 1);
    EXPECT_NE(finalDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(finalDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateDown);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), kPortStateDown);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests when damping is not active on port and next DOWN event activates
// damping on port, start damping and advertise the DOWN event immediately.
TEST_F(PortLinkEventDamperTest, DownEventWhileDampingNotActiveStartsDamping)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    // Set the test scenarios.
    perPortDamperPeer.setAccumulatedPenalty(
            perPortDamperPeer.getSuppressThreshold());
    // Set timestamp of last penalty read pretty close to current time so that
    // decay does not happen a lot.
    perPortDamperPeer.setLastPenaltyUpdateTimestamp(getCurrentTimeUsecs() -
                                                        5000);
    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_UP);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_UP);

    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;
    sai_port_oper_status_notification_t data = {.port_id = kRid,
                                                .port_state = kPortStateDown};

    EXPECT_CALL(*m_notificationHandler,
                onPortStateChangePostLinkEventDamping(
                    /*count=*/1, Pointee(EqOperStatus(data))))
            .WillOnce(Return());

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();
    m_portLinkEventDamper->handlePortStateChange(kPortStateDown);

    EXPECT_TRUE(perPortDamperPeer.getTimerActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_GE(perPortDamperPeer.getAccumulatedPenalty(),
              perPortDamperPeer.getSuppressThreshold());

    struct DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events + 1);
    EXPECT_NE(finalDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(finalDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateDown);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), kPortStateDown);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests when damping is active on port and an UP event is observed, UP event is
// damped.
TEST_F(PortLinkEventDamperTest, UpEventWhileDampingActiveIsDamped)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    activateDampingOnPort(m_portLinkEventDamper, perPortDamperPeer,
                          m_notificationHandler);

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    uint32_t prev_penalty = perPortDamperPeer.getAccumulatedPenalty();
    constexpr sai_port_oper_status_t kPortStateUp = SAI_PORT_OPER_STATUS_UP;

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(/*count=*/1, _))
            .Times(0);

    m_portLinkEventDamper->handlePortStateChange(kPortStateUp);

    EXPECT_TRUE(perPortDamperPeer.getTimerActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_EQ(perPortDamperPeer.getAccumulatedPenalty(), prev_penalty);

    struct DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_up_events,
              initialDampingStats.pre_damping_up_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_up_events,
              initialDampingStats.post_damping_up_events);
    EXPECT_EQ(finalDampingStats.last_advertised_up_event_timestamp,
              initialDampingStats.last_advertised_up_event_timestamp);
    EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateUp);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_DOWN);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests when damping is active on port and an DOWN event is observed, DOWN
// event is damped.
TEST_F(PortLinkEventDamperTest, DownEventWhileDampingActiveIsDamped)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);
    perPortDamperPeer.setDampingConfigEnabled(/*state=*/true);

    activateDampingOnPort(m_portLinkEventDamper, perPortDamperPeer,
                          m_notificationHandler);

    struct DampingStats initialDampingStats =
            m_portLinkEventDamper->getDampingStats();

    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_UP);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_UP);

    constexpr sai_port_oper_status_t kPortStateDown = SAI_PORT_OPER_STATUS_DOWN;

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(/*count=*/1, _))
            .Times(0);

    m_portLinkEventDamper->handlePortStateChange(kPortStateDown);

    EXPECT_TRUE(perPortDamperPeer.getTimerActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingActive());
    EXPECT_TRUE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_GT(perPortDamperPeer.getAccumulatedPenalty(),
              perPortDamperPeer.getReuseThreshold());

    struct DampingStats finalDampingStats =
            m_portLinkEventDamper->getDampingStats();
    EXPECT_EQ(finalDampingStats.pre_damping_down_events,
              initialDampingStats.pre_damping_down_events + 1);
    EXPECT_EQ(finalDampingStats.post_damping_down_events,
              initialDampingStats.post_damping_down_events);
    EXPECT_EQ(finalDampingStats.last_advertised_down_event_timestamp,
              initialDampingStats.last_advertised_down_event_timestamp);
    EXPECT_EQ(perPortDamperPeer.getActualState(), kPortStateDown);
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(), SAI_PORT_OPER_STATUS_UP);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

// Tests if config update is happening with same config that is already running,
// update operation will be no-op.
TEST_F(PortLinkEventDamperTest, UpdatingSameLinkEventDampingConfigIsNoOp)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    m_portLinkEventDamper->updateLinkEventDampingConfig(kDefaultConfig);

    // State after updating same config should not change anything.
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
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

// Update link event damping config.
TEST_P(PortLinkEventDamperDisabledConfigTest, UpdateLinkEventDampingConfigFewTimes)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    // Update the damping config.
    sai_redis_link_event_damping_algo_aied_config_t config = kDefaultConfig;
    config.reuse_threshold = 1100;
    sai_port_oper_status_t currentActualState = perPortDamperPeer.getActualState();

    m_portLinkEventDamper->updateLinkEventDampingConfig(config);

    {
        SCOPED_TRACE(::testing::Message()
                     << "Max suppress time(msec): " << config.max_suppress_time
                     << ", suppress threshold: " << config.suppress_threshold
                     << ", reuse threshold: " << config.reuse_threshold
                     << ", half life(msec): " << config.decay_half_life
                     << ", flap penalty: " << config.flap_penalty);

        verifyStatesAfterLinkEventDampingConfigUpdate(
                perPortDamperPeer, config, currentActualState,
                /*damping_config_enabled=*/true);

        EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(), 3111);
    }

    // Set actual state and  advertise state different to trigger advertisement of
    // state.
    activateDampingOnPort(m_portLinkEventDamper, perPortDamperPeer,
                          m_notificationHandler);

    perPortDamperPeer.setAdvertisedState(SAI_PORT_OPER_STATUS_DOWN);
    perPortDamperPeer.setActualState(SAI_PORT_OPER_STATUS_UP);

    currentActualState = SAI_PORT_OPER_STATUS_UP;

    sai_port_oper_status_notification_t data = {
            .port_id = kRid, .port_state = currentActualState};

    EXPECT_CALL(*m_notificationHandler,
            onPortStateChangePostLinkEventDamping(/*count=*/1, Pointee(EqOperStatus(data))))
            .WillOnce(Return());

    // Update the damping config.
    config.reuse_threshold = 1300;
    config.max_suppress_time = config.decay_half_life * 2;
    m_portLinkEventDamper->updateLinkEventDampingConfig(config);

    {
        SCOPED_TRACE(::testing::Message()
                     << "Max suppress time(msec): " << config.max_suppress_time
                     << ", suppress threshold: " << config.suppress_threshold
                     << ", reuse threshold: " << config.reuse_threshold
                     << ", half life(msec): " << config.decay_half_life
                     << ", flap penalty: " << config.flap_penalty);

        verifyStatesAfterLinkEventDampingConfigUpdate(
                perPortDamperPeer, config, currentActualState,
                /*damping_config_enabled=*/true);

        EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(), 5200);
    }

    // Update with default damping with disabled config.
    config = GetParam();
    m_portLinkEventDamper->updateLinkEventDampingConfig(config);

    {
        SCOPED_TRACE(::testing::Message()
                     << "Max suppress time(msec): " << config.max_suppress_time
                     << ", suppress threshold: " << config.suppress_threshold
                     << ", reuse threshold: " << config.reuse_threshold
                     << ", half life(msec): " << config.decay_half_life
                     << ", flap penalty: " << config.flap_penalty);

        verifyStatesAfterLinkEventDampingConfigUpdate(
                perPortDamperPeer, config, currentActualState,
                /*damping_config_enabled=*/false);

        EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(), 0);
    }

    // Update with default damping config and it will enable the config.
    perPortDamperPeer.setTimerActive(true);
    m_portLinkEventDamper->updateLinkEventDampingConfig(kDefaultConfig);

    {
        SCOPED_TRACE(::testing::Message()
                     << "Max suppress time(msec): " << kDefaultConfig.max_suppress_time
                     << ", suppress threshold: " << kDefaultConfig.suppress_threshold
                     << ", reuse threshold: " << kDefaultConfig.reuse_threshold
                     << ", half life(msec): " << kDefaultConfig.decay_half_life
                     << ", flap penalty: " << kDefaultConfig.flap_penalty);

        verifyStatesAfterLinkEventDampingConfigUpdate(
                perPortDamperPeer, kDefaultConfig, currentActualState,
                /*damping_config_enabled=*/true);

        EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(),
                  kExpectedPenaltyCeiling);
    }

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
}

TEST_P(PortLinkEventDamperDisabledConfigTest,
       LinkEventDampingSetupSucceedsWithDisabledConfig)
{
    PortLinkEventDamperPeer perPortDamperPeer(m_portLinkEventDamper);
    verifyStatesAfterPortDamperCreation(perPortDamperPeer);

    sai_redis_link_event_damping_algo_aied_config_t param = GetParam();
    perPortDamperPeer.setMaxSuppressTime(param.max_suppress_time);
    perPortDamperPeer.setSuppressThreshold(param.suppress_threshold);
    perPortDamperPeer.setReuseThreshold(param.reuse_threshold);
    perPortDamperPeer.setDecayHalfLife(param.decay_half_life);
    perPortDamperPeer.setFlapPenalty(param.flap_penalty);

    EXPECT_CALL(*m_notificationHandler, onPortStateChangePostLinkEventDamping(/*count=*/1, _))
            .Times(0);

    m_portLinkEventDamper->setup();

    EXPECT_EQ(perPortDamperPeer.getPenaltyCeiling(), 0);
    EXPECT_FALSE(perPortDamperPeer.getDampingConfigEnabled());
    EXPECT_EQ(perPortDamperPeer.getAdvertisedState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_EQ(perPortDamperPeer.getActualState(),
              SAI_PORT_OPER_STATUS_UNKNOWN);
    EXPECT_TRUE(perPortDamperPeer.getTimerAddedToSelect());

    setUpLinkEventDamperDestruction(perPortDamperPeer, m_notificationHandler);
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

}  // namespace
}  // namespace syncd
