#pragma once

#include <memory>

#include "DampingStats.h"
#include "NotificationHandler.h"
#include "SelectableEventHandler.h"
#include "sairedis.h"
#include "swss/sal.h"
#include "swss/select.h"
#include "swss/selectabletimer.h"

extern "C" {
#include "saimetadata.h"
}

namespace syncd
{
    class PortLinkEventDamper : public SelectableEventHandler
    {
        public:

            PortLinkEventDamper(
                    _In_ std::shared_ptr<NotificationHandler> notificationHandler,
                    _In_ std::shared_ptr<swss::Select> sel,
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid,
                    _In_ const sai_redis_link_event_damping_algo_aied_config_t &config,
                    _In_ bool monitorOnly = false);

            virtual ~PortLinkEventDamper();

            void setup();

            void handlePortStateChange(
                    _In_ sai_port_oper_status_t newPortState);

            void updateLinkEventDampingConfig(
                    _In_ const sai_redis_link_event_damping_algo_aied_config_t &config,
                    _In_ bool monitorOnly = false);

            void handleSelectableEvent() override;

            DampingStats getDampingStats() const;

            int getSelectableTimerFd();

            bool isActive() const;

            bool isConfigEnabled() const;

            uint32_t getAccumulatedPenalty() const;

            sai_port_oper_status_t getAdvertisedState() const;

            sai_port_oper_status_t getActualState() const;

            uint32_t getPenaltyCeiling() const;

        protected:

            virtual uint64_t getCurrentTimeUsecs() const;

        private:

            void resetTimer(
                    _In_ int64_t time_us);

            void cancelTimer();

            void advertisePortState(
                    _In_ sai_port_oper_status_t portState);

            void updatePenaltyCeiling();

            void updatePenalty();

            bool isConfigSameAsRunningConfig(
                    _In_ const sai_redis_link_event_damping_algo_aied_config_t &config,
                    _In_ bool monitorOnly) const;

            bool configContainsZeroValueParam() const;

            std::shared_ptr<NotificationHandler> m_notificationHandler;

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

            bool m_monitorOnly;

            uint64_t m_lastPenaltyUpdateTimestamp;

            swss::SelectableTimer m_timer;

            bool m_timerAddedToSelect;

            bool m_timerActive;

            sai_port_oper_status_t m_advertisedState;

            sai_port_oper_status_t m_actualState;

            DampingStats m_stats;
    };

}  // namespace syncd
