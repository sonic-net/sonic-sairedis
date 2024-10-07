#pragma once

#include <memory>

#include "NotificationHandler.h"
#include "sairedis.h"
#include "swss/sal.h"
#include "swss/select.h"
#include "swss/selectabletimer.h"

extern "C" {
#include "saimetadata.h"
}

namespace syncd
{

    constexpr uint64_t MICRO_SECS_PER_SEC = 1000000ULL;
    constexpr uint64_t NANO_SECS_PER_MICRO_SEC = 1000ULL;
    constexpr uint64_t MICRO_SECS_PER_MILLI_SEC = 1000ULL;

    inline uint64_t getCurrentTimeUsecs()
    {
        SWSS_LOG_ENTER();

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        return (ts.tv_sec * MICRO_SECS_PER_SEC) +
               (ts.tv_nsec / NANO_SECS_PER_MICRO_SEC);
    }

  struct DampingStats
  {
      // Number of link UP events received.
      uint64_t pre_damping_up_events;
      // Number of link DOWN events received.
      uint64_t pre_damping_down_events;
      // Number of link UP events advertised post damping.
      uint64_t post_damping_up_events;
      // Number of link DOWN events advertised post damping.
      uint64_t post_damping_down_events;
      // Timestamp for last advertised link up event post damping.
      std::string last_advertised_up_event_timestamp;
      // Timestamp for last advertised link down event post damping.
      std::string last_advertised_down_event_timestamp;

      DampingStats()
          : pre_damping_up_events(0ULL),
            pre_damping_down_events(0ULL),
            post_damping_up_events(0ULL),
            post_damping_down_events(0ULL),
            last_advertised_up_event_timestamp("none"),
            last_advertised_down_event_timestamp("none") {}
    };

    // Class to handle the link event damping on a port.
    class PortLinkEventDamper
    {
        public:
            PortLinkEventDamper(
                    _In_ std::shared_ptr<NotificationHandler> notificationHandler,
                    _In_ std::shared_ptr<swss::Select> sel,
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid,
                    _In_ const sai_redis_link_event_damping_algo_aied_config_t &config);

            virtual ~PortLinkEventDamper();

            // Sets up the initial states for link event damping.
            void setup();

            // Handles and processes the port state change event.
            void handlePortStateChange(
                    _In_ sai_port_oper_status_t newPortState);

            // Updates the link event damping configuration on the port.
            void updateLinkEventDampingConfig(
                    _In_ const sai_redis_link_event_damping_algo_aied_config_t &config);

            // Handles for Selectable timer events.
            void handleSelectableEvent();

            struct DampingStats getDampingStats() const;

            int getSelectableTimerFd();

        private:
            // Resets the timer.
            void resetTimer(
                    _In_ int64_t time_us);

            // Cancels the timer by stopping the timer.
            void cancelTimer();

            // Advertises the port state to consumers.
            void advertisePortState(
                    _In_ sai_port_oper_status_t portState);

            void updatePenaltyCeiling();

            void updatePenalty();

            bool isConfigSameAsRunningConfig(
                    _In_ const sai_redis_link_event_damping_algo_aied_config_t &config);

            // Checks if any config parameter value is 0.
            bool configContainsZeroValueParam() const;

            std::shared_ptr<NotificationHandler> m_notificationHandler;

            // This is to add SelectableTimer.
            std::shared_ptr<swss::Select> m_sel;

            sai_object_id_t m_vid;
            sai_object_id_t m_rid;

            uint64_t m_maxSuppressTimeUsec;
            uint64_t m_decayHalfLifeUsec;
            uint32_t m_suppressThreshold;
            uint32_t m_reuseThreshold;
            uint32_t m_flapPenalty;
            uint32_t m_accumulatedPenalty;
            uint32_t m_penaltyCeiling;
            bool m_dampingConfigEnabled;
            bool m_dampingActive;
            uint64_t m_lastPenaltyUpdateTimestamp;

            // Timer to manage link event damping.
            swss::SelectableTimer m_timer;

            // Flag to annotate if timer is added to select.
            bool m_timerAddedToSelect;
            bool m_timerActive;

            // Advertised port's oper status to consumers.
            sai_port_oper_status_t m_advertisedState;

            // Actual port's oper status in the ASIC.
            sai_port_oper_status_t m_actualState;

            // Damping related statistics.
            struct DampingStats m_stats;

            friend class PortLinkEventDamperPeer;
            friend class LinkEventDamperPeer;
    };
}  // namespace syncd
