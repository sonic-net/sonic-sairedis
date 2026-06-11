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
     *
     * WARNING: NONE mode disables ALL meta-layer protections for the
     * allow-listed DASH types. Set this only if the vendor SAI layer is
     * known to perform all expected validations.
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
     *   - PA_VALIDATION_ENTRY
     */
    constexpr sai_object_type_t kDashTypes[] =
    {
        (sai_object_type_t) SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY,
        (sai_object_type_t) SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY,
        (sai_object_type_t) SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY,
        (sai_object_type_t) SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY,
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

    /*
     * This only returns true under NONE. The entry-specific validations
     * in Meta.cpp use this helper so that adding a new policy mode in
     * the future can opt them out from a single place without touching Meta.cpp.
     */
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
        /*
         * NOTE: in EXISTENCE_REFCOUNT mode a non-OID set after create is NOT
         * cached, so a subsequent meta-layer get against the cache would return
         * stale/missing data for that attribute. In practice the vendor SAI layer
         * handles real gets, so this is an intentional trade-off.
         */
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

void saimeta::logDashPolicy()
{
    SWSS_LOG_ENTER();

    std::string types;
    for (auto t : kDashTypes)
    {
        types += sai_serialize_object_type(t) + " ";
    }

    SWSS_LOG_NOTICE("DASH meta-cache policy: mode=%s, object_types=[ %s]",
            dashCacheModeName(),
            types.c_str());
}
