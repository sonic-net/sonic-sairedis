#pragma once

#include <cstdint>
#include <string>

namespace syncd
{
    struct DampingStats
    {
        uint64_t pre_damping_up_events;

        uint64_t pre_damping_down_events;

        uint64_t post_damping_up_events;

        uint64_t post_damping_down_events;

        std::string last_advertised_up_event_timestamp;

        std::string last_advertised_down_event_timestamp;

        DampingStats()
            : pre_damping_up_events(0ULL),
              pre_damping_down_events(0ULL),
              post_damping_up_events(0ULL),
              post_damping_down_events(0ULL),
              last_advertised_up_event_timestamp("none"),
              last_advertised_down_event_timestamp("none") {}
    };

}  // namespace syncd
