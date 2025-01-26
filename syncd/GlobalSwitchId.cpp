#include "GlobalSwitchId.h"

#include "meta/otai_serialize.h"

#include "swss/logger.h"

using namespace syncd;

#ifdef SAITHRIFT

/**
 * @brief Global switch RID used by thrift RPC server.
 */
otai_object_id_t gSwitchId = SAI_NULL_OBJECT_ID;

#endif

void GlobalSwitchId::setSwitchId(
        _In_ otai_object_id_t switchRid)
{
    SWSS_LOG_ENTER();

#ifdef SAITHRIFT

    if (gSwitchId != SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("gSwitchId already initialized!, SAI THRIFT don't support multiple switches yet, FIXME");
    }

    gSwitchId = switchRid;

    SWSS_LOG_NOTICE("Initialize gSwitchId with ID = %s",
            otai_serialize_object_id(gSwitchId).c_str());
#endif

}
