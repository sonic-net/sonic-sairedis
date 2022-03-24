#include "AdvancedRestartTable.h"

using namespace syncd;

AdvancedRestartTable::AdvancedRestartTable(
        _In_ const std::string& dbName)
{
    SWSS_LOG_ENTER();

    m_dbState = std::make_shared<swss::DBConnector>(dbName, 0);

    m_table = std::make_shared<swss::Table>(m_dbState.get(), STATE_ADVANCED_RESTART_TABLE_NAME);
}

AdvancedRestartTable::~AdvancedRestartTable()
{
    SWSS_LOG_ENTER();

    // empty
}

void AdvancedRestartTable::setFlagFailed()
{
    SWSS_LOG_ENTER();

    m_table->hset("advanced-shutdown", "state", "set-flag-failed");
}

void AdvancedRestartTable::setPreShutdown(
        _In_ bool succeeded)
{
    SWSS_LOG_ENTER();

    m_table->hset("advanced-shutdown", "state", succeeded ? "pre-shutdown-succeeded" : "pre-shutdown-failed");
}

void AdvancedRestartTable::setAdvancedShutdown(
        _In_ bool succeeded)
{
    SWSS_LOG_ENTER();

    m_table->hset("advanced-shutdown", "state", succeeded ?  "advanced-shutdown-succeeded": "advanced-shutdown-failed");
}
