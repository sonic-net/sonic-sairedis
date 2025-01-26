#include "Context.h"

#include "swss/logger.h"

#include "meta/otai_serialize.h"

using namespace otairedis;
using namespace std::placeholders;

Context::Context(
        _In_ std::shared_ptr<ContextConfig> contextConfig,
        _In_ std::shared_ptr<Recorder> recorder,
        _In_ std::function<otai_linecard_notifications_t(std::shared_ptr<Notification>, Context*)> notificationCallback):
    m_contextConfig(contextConfig),
    m_recorder(recorder),
    m_notificationCallback(notificationCallback)
{
    SWSS_LOG_ENTER();

    // will create notification thread
    m_redisOtai = std::make_shared<RedisRemoteOtaiInterface>(
            m_contextConfig,
            std::bind(&Context::handle_notification, this, _1),
            m_recorder);

    m_meta = std::make_shared<otaimeta::Meta>(m_redisOtai);

    m_redisOtai->setMeta(m_meta);
}

Context::~Context()
{
    SWSS_LOG_ENTER();

    m_redisOtai->apiUninitialize(); // will stop threads

    m_redisOtai = nullptr;

    m_meta = nullptr;
}

otai_linecard_notifications_t Context::handle_notification(
        _In_ std::shared_ptr<Notification> notification)
{
    SWSS_LOG_ENTER();

    return m_notificationCallback(notification, this);
}

void Context::populateMetadata(
        _In_ otai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    auto& dump = m_redisOtai->getTableDump();

    SWSS_LOG_NOTICE("dump size: %zu", dump.size());

    if (dump.size() == 0)
    {
        SWSS_LOG_NOTICE("skipping populate metadata for switch %s, (probably connecting to already existing switch)",
                otai_serialize_object_id(switchId).c_str());
        return;
    }

    m_meta->populate(dump.at(switchId));
}
