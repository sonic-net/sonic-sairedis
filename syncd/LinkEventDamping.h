#pragma once

#include "sai.h"
#include "sairedis.h"

namespace syncd
{
    /**
     * @brief Link event damping configuration and state per port
     */
    struct LinkEventDampingPortState
    {
        // Configuration parameters
        sai_redis_link_event_damping_algorithm_t algorithm;
        sai_redis_link_event_damping_algo_aied_config_t aied_config;

        // Runtime state for AIED algorithm
        uint32_t current_penalty;           // Current penalty value
        uint64_t last_transition_time_ms;   // Timestamp of last transition (milliseconds)
        uint64_t last_decay_time_ms;        // Timestamp of last decay calculation (milliseconds)
        uint64_t damping_start_time_ms;     // When damping state started (milliseconds)
        bool is_damping_active;             // Whether link is currently in damped state
        sai_port_oper_status_t physical_status;  // Physical port status
        sai_port_oper_status_t advertised_status; // Last advertised status (may differ due to damping)
        sai_port_oper_status_t last_suppressed_status; // Last event suppressed while damping
        bool pending_state_sync;            // Flag to indicate state mismatch needs propagation

        // Counters for monitoring
        uint64_t pre_damping_link_transitions;
        uint64_t pre_damping_up_events;
        uint64_t pre_damping_down_events;
        uint64_t post_damping_up_events;
        uint64_t post_damping_down_events;
        uint64_t post_damping_link_transitions;

        // Constructor with defaults
        LinkEventDampingPortState()
            : algorithm(SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED),
              current_penalty(0),
              last_transition_time_ms(0),
              last_decay_time_ms(0),
              damping_start_time_ms(0),
              is_damping_active(false),
              physical_status(SAI_PORT_OPER_STATUS_UNKNOWN),
              advertised_status(SAI_PORT_OPER_STATUS_UNKNOWN),
              last_suppressed_status(SAI_PORT_OPER_STATUS_UNKNOWN),
              pending_state_sync(false),
              pre_damping_link_transitions(0),
              pre_damping_up_events(0),
              pre_damping_down_events(0),
              post_damping_up_events(0),
              post_damping_down_events(0),
              post_damping_link_transitions(0)
        {
            // Initialize AIED config with defaults
            aied_config.max_suppress_time = 0;
            aied_config.suppress_threshold = 0;
            aied_config.reuse_threshold = 0;
            aied_config.decay_half_life = 0;
            aied_config.flap_penalty = 0;
        }
    };
}
