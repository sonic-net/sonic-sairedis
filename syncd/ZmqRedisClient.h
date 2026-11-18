#pragma once

#include "RedisClient.h"

#include "swss/asyncdbupdater.h"
#include "swss/dbconnector.h"

#include <memory>

namespace syncd
{
    class ZmqRedisClient : public RedisClient
    {
        public:

            ZmqRedisClient(
                    _In_ std::shared_ptr<swss::DBConnector> dbAsic);

            virtual ~ZmqRedisClient() = default;

            virtual void removeAsicObject(
                    _In_ sai_object_id_t objectVid) const override;

            virtual void removeAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey) override;

            virtual void removeAsicObjects(
                    _In_ const std::vector<std::string>& keys) override;

            virtual void createAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::vector<swss::FieldValueTuple>& attrs) override;

            virtual void createAsicObjects(
                    _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash) override;

            virtual void setAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::string& attr,
                    _In_ const std::string& value) override;

        private:

            std::shared_ptr<swss::AsyncDBUpdater> m_asyncDbUpdater;
    };
}
