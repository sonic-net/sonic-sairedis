#pragma once

#include "swss/sal.h"
#include "swss/dbconnector.h"
#include "swss/table.h"

#include <string>
#include <memory>

namespace syncd
{
    class AdvancedRestartTable
    {
        public:

            AdvancedRestartTable(
                    _In_ const std::string& dbName);

            virtual ~AdvancedRestartTable();

        public:

            void setFlagFailed();

            void setPreShutdown(
                    _In_ bool succeeded);

            void setAdvancedShutdown(
                    _In_ bool succeeded);

        private:

            std::shared_ptr<swss::DBConnector> m_dbState;

            std::shared_ptr<swss::Table> m_table;
    };
}
