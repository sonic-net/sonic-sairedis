#include "PortLinkEventDamper.h"

#include <algorithm>
#include <cmath>

#include "Utils.h"
#include "meta/sai_serialize.h"
#include "swss/logger.h"
#include "swss/timestamp.h"

using namespace syncd;

PortLinkEventDamper::PortLinkEventDamper(
        _In_ std::shared_ptr<NotificationHandler> notificationHandler,
        _In_ std::shared_ptr<swss::Select> sel,
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid,
        _In_ const sai_redis_link_event_damping_algo_aied_config_t &config)
    : m_notificationHandler(notificationHandler),
      m_sel(sel),
      m_vid(vid),
      m_rid(rid),
      m_maxSuppressTimeUsec(uint64_t(config.max_suppress_time) *
                              MICRO_SECS_PER_MILLI_SEC),
      m_decayHalfLifeUsec(uint64_t(config.decay_half_life) *
                            MICRO_SECS_PER_MILLI_SEC),
      m_suppressThreshold(config.suppress_threshold),
      m_reuseThreshold(config.reuse_threshold),
      m_flapPenalty(config.flap_penalty),
      m_accumulatedPenalty(0),
      m_penaltyCeiling(std::numeric_limits<uint32_t>::max()),
      m_dampingConfigEnabled(false),
      m_dampingActive(false),
      m_lastPenaltyUpdateTimestamp(0),
      m_timer(/*interval=*/timespec{.tv_sec = 0, .tv_nsec = 0}),
      m_timerAddedToSelect(false),
      m_timerActive(false),
      m_advertisedState(SAI_PORT_OPER_STATUS_UNKNOWN),
      m_actualState(SAI_PORT_OPER_STATUS_UNKNOWN),
      m_stats()
{
    SWSS_LOG_ENTER();
}

PortLinkEventDamper::~PortLinkEventDamper()
{
    SWSS_LOG_ENTER();

    // Clean up the resources.
    if (m_timerAddedToSelect)
    {
        cancelTimer();
        m_sel->removeSelectable(&m_timer);

        SWSS_LOG_DEBUG("Removed timer with fd %d for port vid: %s from select.",
                       m_timer.getFd(), sai_serialize_object_id(m_vid).c_str());
    }

    advertisePortState(m_actualState);
}

void PortLinkEventDamper::setup()
{
    SWSS_LOG_ENTER();

    if (configContainsZeroValueParam())
    {
        SWSS_LOG_NOTICE(
                "Damping config is disabled on port vid: %s due to one or more "
                "received config params being 0.",
                sai_serialize_object_id(m_vid).c_str());

        m_penaltyCeiling = 0;
        m_dampingConfigEnabled = false;
    }
    else
    {
        updatePenaltyCeiling();
        m_dampingConfigEnabled = true;
    }

    m_sel->addSelectable(&m_timer);
    m_timer.stop();

    SWSS_LOG_DEBUG("Added timer with fd %d for port vid: %s to select.",
                   m_timer.getFd(), sai_serialize_object_id(m_vid).c_str());

    m_timerAddedToSelect = true;
}

void PortLinkEventDamper::updatePenaltyCeiling()
{
    SWSS_LOG_ENTER();

    // If any of config param value is 0, damping config is treated as disabled
    // and calculation of penalty ceiling is not valid so mark penalty ceiling 0.
    if (configContainsZeroValueParam())
    {
        m_penaltyCeiling = 0;

        return;
    }

    double ceiling = pow(2.0, double(m_maxSuppressTimeUsec) /
                                    double(m_decayHalfLifeUsec)) *
                                    double(m_reuseThreshold);

    if (uint64_t(ceiling) > uint64_t(std::numeric_limits<uint32_t>::max()))
    {
        SWSS_LOG_NOTICE("Calculated ceiling %ull greater than %u, truncating",
                        uint64_t(ceiling), std::numeric_limits<uint32_t>::max());

        m_penaltyCeiling = std::numeric_limits<uint32_t>::max();
    }
    else
    {
        m_penaltyCeiling = uint32_t(ceiling);
    }

    SWSS_LOG_INFO("Penalty ceiling %u on port vid: %s.", m_penaltyCeiling,
                  sai_serialize_object_id(m_vid).c_str());
}

void PortLinkEventDamper::updatePenalty()
{
    SWSS_LOG_ENTER();

    uint64_t currentTime = getCurrentTimeUsecs();

    if (m_accumulatedPenalty > 0)
    {
        m_accumulatedPenalty = sairedis::Utils::valueAfterDecay(
                currentTime - m_lastPenaltyUpdateTimestamp, m_decayHalfLifeUsec,
                m_accumulatedPenalty);
    }

    m_lastPenaltyUpdateTimestamp = currentTime;

    // Ensure updated accumulated penalty does not cross penalty ceiling.
    // To avoid integer overflow, first check if enough gap is available.
    if (m_penaltyCeiling - m_accumulatedPenalty > m_flapPenalty)
    {
        m_accumulatedPenalty += m_flapPenalty;
    }
    else
    {
        m_accumulatedPenalty = m_penaltyCeiling;
    }
}

void PortLinkEventDamper::resetTimer(
        _In_ int64_t time_us)
{
    SWSS_LOG_ENTER();

    m_timer.setInterval(
        /*interval=*/{.tv_sec = (time_us / int64_t(MICRO_SECS_PER_SEC)),
                      .tv_nsec = (time_us % int64_t(MICRO_SECS_PER_SEC)) *
                               int64_t(NANO_SECS_PER_MICRO_SEC)});
    m_timer.reset();
    m_timerActive = true;
}

void PortLinkEventDamper::cancelTimer()
{
    SWSS_LOG_ENTER();

    m_timer.stop();
    m_timerActive = false;
}

void PortLinkEventDamper::handleSelectableEvent()
{
    SWSS_LOG_ENTER();

    cancelTimer();

    // Calculate new penalty after decay.
    uint64_t currentTime = getCurrentTimeUsecs();
    m_accumulatedPenalty = sairedis::Utils::valueAfterDecay(
        currentTime - m_lastPenaltyUpdateTimestamp, m_decayHalfLifeUsec,
        m_accumulatedPenalty);
    m_lastPenaltyUpdateTimestamp = currentTime;

    // If accumulated penalty after the decay is below link reuse threshold, port
    // no longer needs to be damped. Else, continue damping the port.
    if (m_accumulatedPenalty <= m_reuseThreshold)
    {
        m_dampingActive = false;

        SWSS_LOG_INFO(
                "Damping stops at time %llu on port vid: %s with accumulated penalty: "
                "%u, link reuse threshold: %u.",
                m_lastPenaltyUpdateTimestamp, sai_serialize_object_id(m_vid).c_str(),
                m_accumulatedPenalty, m_reuseThreshold);

        // Advertise the actual state immediately.
        advertisePortState(m_actualState);
    }
    else
    {
        uint64_t tv_usec = sairedis::Utils::timeToReachTargetValueUsingHalfLife(
                m_decayHalfLifeUsec, m_accumulatedPenalty, m_reuseThreshold);

        SWSS_LOG_NOTICE(
                "Expected current accumulated penalty %u to be <= link reuse threshold "
                "%u "
                "for port vid: %s, need another %d usecs.",
                m_accumulatedPenalty, m_reuseThreshold,
                sai_serialize_object_id(m_vid).c_str(), tv_usec);

        resetTimer(tv_usec);
    }
}

void PortLinkEventDamper::handlePortStateChange(
        _In_ sai_port_oper_status_t newPortState)
{
    SWSS_LOG_ENTER();

    m_actualState = newPortState;

    if (newPortState == SAI_PORT_OPER_STATUS_UP)
    {
        ++m_stats.pre_damping_up_events;
    }
    else
    {
        ++m_stats.pre_damping_down_events;
    }

    // If link event damping config is not enabled, advertise the port event.
    if (m_dampingConfigEnabled == false)
    {
        advertisePortState(newPortState);

        return;
    }

    if (m_dampingActive == false)
    {
        advertisePortState(newPortState);

        // Penalty is updated only for DOWN events.
        if (newPortState == SAI_PORT_OPER_STATUS_DOWN)
        {
            updatePenalty();

            if (m_accumulatedPenalty >= m_suppressThreshold)
            {
                // Start the damping on the port.
                resetTimer(sairedis::Utils::timeToReachTargetValueUsingHalfLife(
                           m_decayHalfLifeUsec, m_accumulatedPenalty, m_reuseThreshold));
                m_dampingActive = true;

                SWSS_LOG_INFO(
                        "Damping starts at time %llu on port vid: %s with accumulated "
                        "penalty: %u, reuse_threshold: %u",
                        m_lastPenaltyUpdateTimestamp,
                        sai_serialize_object_id(m_vid).c_str(), m_accumulatedPenalty,
                        m_reuseThreshold);
            }
        }
    }
    else // Damping is active on port.
    {
        // UP event on a damped link is no-op.
        if (newPortState == SAI_PORT_OPER_STATUS_DOWN)
        {
            // Cancel the timer on the port.
            cancelTimer();
            updatePenalty();

            // Restart the timer.
            resetTimer(sairedis::Utils::timeToReachTargetValueUsingHalfLife(
                       m_decayHalfLifeUsec, m_accumulatedPenalty, m_reuseThreshold));
        }
    }
}

void PortLinkEventDamper::advertisePortState(
        _In_ sai_port_oper_status_t portState)
{
    SWSS_LOG_ENTER();

    if (portState != m_advertisedState)
    {
        sai_port_oper_status_notification_t data = {.port_id = m_rid,
                                                    .port_state = portState};

        SWSS_LOG_NOTICE("Advertising [port rid: %s, port oper status: %s]",
                sai_serialize_object_id(data.port_id).c_str(),
                sai_serialize_port_oper_status(data.port_state).c_str());

        m_notificationHandler->onPortStateChangePostLinkEventDamping(/*count=*/1, &data);
        m_advertisedState = portState;

        if (portState == SAI_PORT_OPER_STATUS_UP)
        {
            ++m_stats.post_damping_up_events;
            m_stats.last_advertised_up_event_timestamp = swss::getTimestamp();
        }
        else
        {
            ++m_stats.post_damping_down_events;
            m_stats.last_advertised_down_event_timestamp = swss::getTimestamp();
        }
    }
}

void PortLinkEventDamper::updateLinkEventDampingConfig(
        _In_ const sai_redis_link_event_damping_algo_aied_config_t &config)
{
    SWSS_LOG_ENTER();

    if (isConfigSameAsRunningConfig(config))
    {
        return;
    }

    m_maxSuppressTimeUsec =
            uint64_t(config.max_suppress_time) * MICRO_SECS_PER_MILLI_SEC;
    m_decayHalfLifeUsec =
            uint64_t(config.decay_half_life) * MICRO_SECS_PER_MILLI_SEC;
    m_suppressThreshold = config.suppress_threshold;
    m_reuseThreshold = config.reuse_threshold;
    m_flapPenalty = config.flap_penalty;
    m_accumulatedPenalty = 0;
    m_dampingActive = false;
    m_lastPenaltyUpdateTimestamp = getCurrentTimeUsecs();

    if (configContainsZeroValueParam())
    {
        if (m_dampingConfigEnabled)
        {
            SWSS_LOG_NOTICE("Disabling the damping config on port vid: %s",
                    sai_serialize_object_id(m_vid).c_str());
        }

        m_penaltyCeiling = 0;
        m_dampingConfigEnabled = false;
    }
    else
    {
        updatePenaltyCeiling();

        if (m_dampingConfigEnabled == false)
        {
            SWSS_LOG_NOTICE("Enabling the damping config on port vid: %s",
                    sai_serialize_object_id(m_vid).c_str());
        }

        m_dampingConfigEnabled = true;
    }

    if (m_timerActive)
    {
        cancelTimer();
    }

    if (m_advertisedState != m_actualState)
    {
        // Advertise the current state.
        advertisePortState(m_actualState);
    }

    SWSS_LOG_DEBUG("Link event damping config on port vid: %s is updated.",
                    sai_serialize_object_id(m_vid).c_str());
}

bool PortLinkEventDamper::isConfigSameAsRunningConfig(
        _In_ const sai_redis_link_event_damping_algo_aied_config_t &config)
{
    SWSS_LOG_ENTER();

    if ((config.max_suppress_time !=
         uint32_t(m_maxSuppressTimeUsec / MICRO_SECS_PER_MILLI_SEC)) ||
        (config.decay_half_life !=
         uint32_t(m_decayHalfLifeUsec / MICRO_SECS_PER_MILLI_SEC)) ||
        (config.suppress_threshold != m_suppressThreshold) ||
        (config.reuse_threshold != m_reuseThreshold) ||
        (config.flap_penalty != m_flapPenalty))
    {
        return false;
    }

    return true;
}

bool PortLinkEventDamper::configContainsZeroValueParam() const
{
    SWSS_LOG_ENTER();

    if ((m_maxSuppressTimeUsec == 0) || (m_decayHalfLifeUsec == 0) ||
        (m_suppressThreshold == 0) || (m_reuseThreshold == 0) ||
        (m_flapPenalty == 0))
    {
        return true;
    }

    return false;
}

struct DampingStats PortLinkEventDamper::getDampingStats() const
{
    SWSS_LOG_ENTER();

    return m_stats;
}

int PortLinkEventDamper::getSelectableTimerFd()
{
    SWSS_LOG_ENTER();

    return m_timer.getFd();
}
