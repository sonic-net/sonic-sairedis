#pragma once

#include "SelectableChannel.h"

#include "swss/consumertable.h"
#include "swss/producertable.h"

namespace sairedis
{
    class RedisSelectableChannel:
        public SelectableChannel
    {
        public:

            RedisSelectableChannel(
                    _In_ std::shared_ptr<swss::DBConnector> dbAsic,
                    _In_ const std::string& asicStateTable,
                    _In_ const std::string& getResponseTable,
                    _In_ const std::string& tempPrefix,
                    _In_ bool modifyRedis);

            virtual ~RedisSelectableChannel() = default;

        public: // SelectableChannel overrides

            virtual bool empty() override;

            virtual void pop(
                    _Out_ swss::KeyOpFieldsValuesTuple& kco,
                    _In_ bool initViewMode) override;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& op) override;

        public: // Selectable overrides

            virtual int getFd() override;

            virtual uint64_t readData() override;

            virtual bool hasData() override;

            virtual bool hasCachedData() override;

            virtual bool initializedWithData() override;

            virtual void updateAfterRead() override;

            virtual int getPri() const override;

        private:

            std::shared_ptr<swss::DBConnector> m_dbAsic;

            std::shared_ptr<swss::ConsumerTable> m_asicState;

            std::shared_ptr<swss::ProducerTable> m_getResponse;

            std::string m_tempPrefix;

            bool m_modifyRedis;
    };
}
