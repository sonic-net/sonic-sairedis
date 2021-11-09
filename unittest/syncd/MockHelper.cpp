#include "VidManager.h"
#include "swss/dbconnector.h"
#include "swss/table.h"

namespace test_syncd {
    sai_object_type_t mock_objectTypeQuery_result;
    void mockVidManagerObjectTypeQuery(sai_object_type_t mock_result)
    {
        mock_objectTypeQuery_result = mock_result;
    }
}

namespace syncd {
    sai_object_type_t VidManager::objectTypeQuery(_In_ sai_object_id_t objectId)
    {
        return test_syncd::mock_objectTypeQuery_result;
    }
}

/*
namespace swss {
    DBConnector::DBConnector(const std::string& dbName, unsigned int timeout, bool isTcpConn)
    {
    }

    DBConnector *DBConnector::newConnector(unsigned int timeout) const
    {
        return nullptr;
    }

    Table::Table(RedisPipeline *pipeline, const std::string &tableName, bool buffered)
    {
    }


}*/
