#include "DashMeta.h"

#include "swss/logger.h"
#include "sai_serialize.h"

extern "C" {
#include "saiextensions.h"
}

using namespace saimeta;

namespace
{
    /*
     * Meta-cache policy for DASH SAI entry types only. Non-DASH object
     * types are unaffected and always behave as FULL.
     *
     *   FULL                cache every attribute (upstream default)
     *   EXISTENCE_REFCOUNT  cache key + OID attrs only (OidRefCounter
     *                       still works; non-OID payloads not deep-copied)
     *   NONE                pass-through, skip all meta bookkeeping
     */
    enum class DashCacheMode
    {
        FULL,
        EXISTENCE_REFCOUNT,
        NONE,
    };

    constexpr DashCacheMode kDashCacheMode = DashCacheMode::EXISTENCE_REFCOUNT;

    /*
     * Hard-coded allow-list of DASH object types this policy applies to.
     *
     * Scoped to the three high-volume DASH entry types that dominate
     * orchagent's meta-cache footprint at smart-switch scale (issue #26683):
     *
     *   - OUTBOUND_CA_TO_PA_ENTRY
     *   - OUTBOUND_ROUTING_ENTRY
     *   - INBOUND_ROUTING_ENTRY
     */
    constexpr sai_object_type_t kDashTypes[] =
    {
        (sai_object_type_t) SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY,
        (sai_object_type_t) SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY,
        (sai_object_type_t) SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY,
    };
}

bool saimeta::isDashObjectType(sai_object_type_t ot)
{
    for (auto t : kDashTypes)
    {
        if (t == ot)
        {
            return true;
        }
    }
    return false;
}

bool saimeta::bypassValidation(sai_object_type_t ot)
{
    if (!isDashObjectType(ot))
    {
        return false;
    }

    SWSS_LOG_DEBUG("DASH policy active for object type %s (mode=%s)",
            sai_serialize_object_type(ot).c_str(),
            dashCacheModeName());

    return kDashCacheMode == DashCacheMode::NONE;
}

bool saimeta::shouldCacheAttribute(
        sai_object_type_t ot,
        const sai_attr_metadata_t& md)
{
    if (!isDashObjectType(ot))
    {
        return true;
    }

    switch (kDashCacheMode)
    {
        case DashCacheMode::FULL:               return true;
        case DashCacheMode::EXISTENCE_REFCOUNT: return md.isoidattribute;
        case DashCacheMode::NONE:               return false;
    }
    return true;
}

const char* saimeta::dashCacheModeName()
{
    switch (kDashCacheMode)
    {
        case DashCacheMode::FULL:               return "FULL";
        case DashCacheMode::EXISTENCE_REFCOUNT: return "EXISTENCE_REFCOUNT";
        case DashCacheMode::NONE:               return "NONE";
    }
    return "UNKNOWN";
}
