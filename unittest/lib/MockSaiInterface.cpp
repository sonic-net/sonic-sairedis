#include "MockSaiInterface.h"
#include "swss/logger.h"

MockSaiInterface::MockSaiInterface()
{
    SWSS_LOG_ENTER();
}

MockSaiInterface::~MockSaiInterface()
{
    SWSS_LOG_ENTER();
}

sai_status_t MockSaiInterface::apiInitialize(
    _In_ uint64_t flags,
    _In_ const sai_service_method_table_t *service_method_table)
{
    SWSS_LOG_ENTER();
    return SAI_STATUS_SUCCESS;
}

sai_status_t MockSaiInterface::apiUninitialize()
{
    SWSS_LOG_ENTER();
    return SAI_STATUS_SUCCESS;
}
