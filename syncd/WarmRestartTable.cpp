#include "WarmRestartTable.h"

using namespace syncd;

WarmRestartTable::WarmRestartTable(
        _In_ const std::string& dbName)
{
    SWSS_LOG_ENTER();

    m_dbState = std::make_shared<swss::DBConnector>(dbName, 0);

    m_table = std::make_shared<swss::Table>(m_dbState.get(), STATE_WARM_RESTART_TABLE_NAME);
}

WarmRestartTable::~WarmRestartTable()
{
    SWSS_LOG_ENTER();

    // empty
}

void WarmRestartTable::setFlagFailed()
{
    SWSS_LOG_ENTER();

    m_table->hset("warm-shutdown", "state", "set-flag-failed");
}

void WarmRestartTable::setPreShutdown(
        _In_ bool succeeded)
{
    SWSS_LOG_ENTER();

    m_table->hset("warm-shutdown", "state", succeeded ? "pre-shutdown-succeeded" : "pre-shutdown-failed");
}

void WarmRestartTable::setWarmShutdown(
        _In_ bool succeeded)
{
    SWSS_LOG_ENTER();

    m_table->hset("warm-shutdown", "state", succeeded ?  "warm-shutdown-succeeded": "warm-shutdown-failed");
}
