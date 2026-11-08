#include <memory>

#include <swss/dbconnector.h>
#include <swss/redisreply.h>
#include <swss/logger.h>

#include "RequestShutdown.h"

#include "TestSyncdLib.h"

void flushDb(std::string name)
{
    SWSS_LOG_ENTER();

    auto db = std::make_shared<swss::DBConnector>(name, 0, true);
    auto reply = std::make_shared<swss::RedisReply>(db.get(), "FLUSHALL", REDIS_REPLY_STATUS);

    reply->checkStatusOK();

    SWSS_LOG_NOTICE("Flushed %s", name.c_str());
}

void flushAsicDb()
{
    SWSS_LOG_ENTER();

    flushDb("ASIC_DB");
}

void flushFlexCounterDb()
{
    SWSS_LOG_ENTER();

    flushDb("FLEX_COUNTER_DB");
}
void sendSyncdShutdownNotification()
{
    SWSS_LOG_ENTER();

    auto opt = std::make_shared<syncd::RequestShutdownCommandLineOptions>();
    opt->setRestartType(syncd::SYNCD_RESTART_TYPE_COLD);

    auto req = std::make_shared<syncd::RequestShutdown>(opt);
    req->send();

    SWSS_LOG_NOTICE("Sent syncd shutdown request");
}
