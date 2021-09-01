#include "RedisNotificationProducer.h"

#include "sairediscommon.h"

#include "swss/logger.h"

using namespace syncd;

RedisNotificationProducer::RedisNotificationProducer(
        _In_ const std::string& dbName)
{
    SWSS_LOG_ENTER();

    m_db = std::make_shared<swss::DBConnector>(dbName, 0);

    m_notificationProducer = std::make_shared<swss::NotificationProducer>(m_db.get(), REDIS_TABLE_NOTIFICATIONS);
}

void RedisNotificationProducer::send(
        _In_ const std::string& op,
        _In_ const std::string& data,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> vals = values;

    m_notificationProducer->send(op, data, vals);
}
