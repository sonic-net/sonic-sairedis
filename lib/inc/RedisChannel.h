#pragma once

#include "Channel.h"
#include "RemoteSaiInterface.h"
#include "SwitchContainer.h"
#include "VirtualObjectIdManager.h"
#include "Recorder.h"
#include "SkipRecordAttrContainer.h"

#include "meta/Notification.h"

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"

#include <memory>
#include <functional>

namespace sairedis
{
    class RedisChannel:
        public Channel
    {
        public:

            RedisChannel(
                    _In_ const std::string& dbAsic,
                    _In_ Channel::Callback callback);

            virtual ~RedisChannel();

        public:

            std::shared_ptr<swss::DBConnector> getDbConnector() const;

            virtual void setBuffered(
                    _In_ bool buffered) override;

            virtual void flush() override;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& command) override;

            virtual void del(
                    _In_ const std::string& key,
                    _In_ const std::string& command) override;

            virtual sai_status_t wait(
                    _In_ const std::string& command,
                    _Out_ swss::KeyOpFieldsValuesTuple& kco) override;

        protected:

            virtual void notificationThreadFunction() override;

        private:

            std::string m_dbAsic;

            /**
             * @brief Asic state channel.
             *
             * Used to sent commands like create/remove/set/get to syncd.
             */
            std::shared_ptr<swss::ProducerTable>  m_asicState;

            /**
             * @brief Get consumer.
             *
             * Channel used to receive responses from syncd.
             */
            std::shared_ptr<swss::ConsumerTable> m_getConsumer;

            std::shared_ptr<swss::DBConnector> m_db;

            std::shared_ptr<swss::RedisPipeline> m_redisPipeline;

        private: // notification

            /**
             * @brief Database connector used for notifications.
             */
            std::shared_ptr<swss::DBConnector> m_dbNtf;

            /**
             * @brief Notification consumer.
             */
            std::shared_ptr<swss::NotificationConsumer> m_notificationConsumer;
    };
}
