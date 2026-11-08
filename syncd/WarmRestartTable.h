#pragma once

#include "swss/sal.h"
#include "swss/dbconnector.h"
#include "swss/table.h"

#include <string>
#include <memory>

namespace syncd
{
    class WarmRestartTable
    {
        public:

            WarmRestartTable(
                    _In_ const std::string& dbName);

            virtual ~WarmRestartTable();

        public:

            void setFlagFailed();

            void setPreShutdown(
                    _In_ bool succeeded);

            void setWarmShutdown(
                    _In_ bool succeeded);

        private:

            std::shared_ptr<swss::DBConnector> m_dbState;

            std::shared_ptr<swss::Table> m_table;
    };
}
