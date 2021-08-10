#pragma once

#include "meta/SaiInterface.h"

namespace sairedis
{
    class RemoteSaiInterface:
        public SaiInterface
    {
        public:

            RemoteSaiInterface() = default;

            virtual ~RemoteSaiInterface() = default;

        public:

            /**
             * @brief Notify syncd API.
             */
            virtual sai_status_t notifySyncd(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_redis_notify_syncd_t redisNotifySyncd) = 0;
    };
}
