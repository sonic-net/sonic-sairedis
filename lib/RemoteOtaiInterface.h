#pragma once

#include "otairedis.h"

#include "meta/OtaiInterface.h"

namespace otairedis
{
    class RemoteOtaiInterface:
        public OtaiInterface
    {
        public:

            RemoteOtaiInterface() = default;

            virtual ~RemoteOtaiInterface() = default;

        public:

            /**
             * @brief Notify syncd API.
             */
            virtual otai_status_t notifySyncd(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_redis_notify_syncd_t redisNotifySyncd) = 0;
    };
}
