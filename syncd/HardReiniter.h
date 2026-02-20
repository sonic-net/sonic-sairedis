#pragma once

#include "meta/SaiInterface.h"
#include "SaiSwitch.h"
#include "VirtualOidTranslator.h"
#include "BaseRedisClient.h"
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
            typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;

        public:

            HardReiniter(
                    _In_ std::shared_ptr<BaseRedisClient> client,
                    _In_ std::shared_ptr<VirtualOidTranslator> translator,
                    _In_ std::shared_ptr<sairedis::SaiInterface> sai,
                    _In_ std::shared_ptr<NotificationHandler> handler);

            virtual ~HardReiniter();

        public:

            std::map<sai_object_id_t, std::shared_ptr<syncd::SaiSwitch>> hardReinit();

        private:

            void readAsicState();

            void redisSetVidAndRidMap(
                    _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t> &map);

        private:

            ObjectIdMap m_vidToRidMap;
            ObjectIdMap m_ridToVidMap;

            std::map<sai_object_id_t, ObjectIdMap> m_switchVidToRid;
            std::map<sai_object_id_t, ObjectIdMap> m_switchRidToVid;

            std::map<sai_object_id_t, std::vector<std::string>> m_switchMap;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            std::shared_ptr<VirtualOidTranslator> m_translator;

            std::shared_ptr<BaseRedisClient> m_client;

            std::shared_ptr<NotificationHandler> m_handler;
    };
}
