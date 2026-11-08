#include "RequestShutdown.h"
#include "ContextConfigContainer.h"

#include "swss/logger.h"
#include "swss/notificationproducer.h"

#include <iostream>

using namespace syncd;

RequestShutdown::RequestShutdown(
        _In_ std::shared_ptr<RequestShutdownCommandLineOptions> options):
    m_options(options)
{
    SWSS_LOG_ENTER();

    auto ccc = sairedis::ContextConfigContainer::loadFromFile(m_options->m_contextConfig.c_str());

    m_contextConfig = ccc->get(m_options->m_globalContext);

    if (m_contextConfig == nullptr)
    {
        SWSS_LOG_THROW("no context config defined at global context %u", m_options->m_globalContext);
    }
}

RequestShutdown::~RequestShutdown()
{
    SWSS_LOG_ENTER();

    // empty
}

void RequestShutdown::send()
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(m_contextConfig->m_dbAsic, 0);

    swss::NotificationProducer restartQuery(&db, SYNCD_NOTIFICATION_CHANNEL_RESTARTQUERY_PER_DB(m_contextConfig->m_dbAsic));

    std::vector<swss::FieldValueTuple> values;

    auto op = RequestShutdownCommandLineOptions::restartTypeToString(m_options->getRestartType());

    SWSS_LOG_NOTICE("requested %s shutdown", op.c_str());

    std::cerr << "requested " << op << " shutdown" << std::endl;

    restartQuery.send(op, op, values);
}
