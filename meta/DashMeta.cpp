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
     * Meta-cache policy for DASH object types only. Non-DASH objects
     * always behave as FULL.
     *
     *   FULL                cache every attribute
     *   EXISTENCE_REFCOUNT  cache key + OID attrs only
     *   NONE                pass-through, no caching
     */
    enum class DashCacheMode
    {
        FULL,
        EXISTENCE_REFCOUNT,
        NONE,
    };

    constexpr DashCacheMode kDashCacheMode = DashCacheMode::EXISTENCE_REFCOUNT;

    /*
     * Hard-coded list of DASH object types that cache policies apply to.
     * Currently scoped to the high-volume DASH entry types identified in Issue #26683
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
    // SWSS_LOG_ENTER() is disabled for performance reasons
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
    // SWSS_LOG_ENTER() is disabled for performance reasons
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
    // SWSS_LOG_ENTER() is disabled for performance reasons
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
    // SWSS_LOG_ENTER() is disabled for performance reasons
    switch (kDashCacheMode)
    {
        case DashCacheMode::FULL:               return "FULL";
        case DashCacheMode::EXISTENCE_REFCOUNT: return "EXISTENCE_REFCOUNT";
        case DashCacheMode::NONE:               return "NONE";
    }
    return "UNKNOWN";
}
