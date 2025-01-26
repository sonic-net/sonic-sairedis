#pragma once

#include "meta/OtaiInterface.h"
#include "OtaiSwitch.h"
#include "VirtualOidTranslator.h"
#include "RedisClient.h"
#include "NotificationHandler.h"

#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>

namespace syncd
{
    class HardReiniter
    {
        public:

            typedef std::unordered_map<std::string, std::string> StringHash;
            typedef std::unordered_map<otai_object_id_t, otai_object_id_t> ObjectIdMap;

        public:

            HardReiniter(
                    _In_ std::shared_ptr<RedisClient> client,
                    _In_ std::shared_ptr<VirtualOidTranslator> translator,
                    _In_ std::shared_ptr<otairedis::OtaiInterface> otai,
                    _In_ std::shared_ptr<NotificationHandler> handler);

            virtual ~HardReiniter();

        public:

            std::map<otai_object_id_t, std::shared_ptr<syncd::OtaiSwitch>> hardReinit();

        private:

            void readAsicState();

            void redisSetVidAndRidMap(
                    _In_ const std::unordered_map<otai_object_id_t, otai_object_id_t> &map);

        private:

            ObjectIdMap m_vidToRidMap;
            ObjectIdMap m_ridToVidMap;

            std::map<otai_object_id_t, ObjectIdMap> m_switchVidToRid;
            std::map<otai_object_id_t, ObjectIdMap> m_switchRidToVid;

            std::map<otai_object_id_t, std::vector<std::string>> m_switchMap;

            std::shared_ptr<otairedis::OtaiInterface> m_vendorSai;

            std::shared_ptr<VirtualOidTranslator> m_translator;

            std::shared_ptr<RedisClient> m_client;

            std::shared_ptr<NotificationHandler> m_handler;
    };
}
