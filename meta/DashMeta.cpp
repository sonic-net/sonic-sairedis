#include "DashMeta.h"

#include "swss/logger.h"
#include "sai_serialize.h"

extern "C" {
#include "saiextensions.h"
}

using namespace saimeta;

/*
 * Hard-coded allow-list of DASH object types this policy applies to.
 *
 * Scoped to the three high-volume DASH entry types that dominate
 * orchagent's meta-cache footprint at smart-switch scale (issue #26683):
 *
 *   - OUTBOUND_CA_TO_PA_ENTRY
 *   - OUTBOUND_ROUTING_ENTRY
 *   - INBOUND_ROUTING_ENTRY
 *
 * All other object types (DASH or not) are unaffected and always behave
 * as DashCacheMode::FULL.
 */
static const sai_object_type_t kDashTypes[] =
{
    (sai_object_type_t) SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY,
    (sai_object_type_t) SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY,
    (sai_object_type_t) SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY,
};

bool saimeta::isDashObjectType(sai_object_type_t ot)
{
    SWSS_LOG_ENTER();

    for (auto t : kDashTypes)
    {
        if (t == ot)
        {
            SWSS_LOG_DEBUG("DASH policy active for object type %s",
                    sai_serialize_object_type(ot).c_str());
            return true;
        }
    }
    return false;
}

DashCacheMode saimeta::getDashCacheMode()
{
    SWSS_LOG_ENTER();

    static const DashCacheMode mode = []() {
       return DashCacheMode::EXISTENCE_REFCOUNT;
    }();
    return mode;
}
