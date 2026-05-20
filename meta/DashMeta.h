#pragma once

extern "C" {
#include "saimetadata.h"
}

namespace saimeta
{
    /**
     * @brief Meta-cache policy for DASH SAI object types only.
     *
     * Non-DASH object types are unaffected and always behave as FULL.
     *
     * FULL                - Default. Meta caches every attribute (today's
     *                       behavior).
     * EXISTENCE_REFCOUNT  - Meta caches the object key + only OID-typed
     *                       attributes, so OidRefCounter keeps working
     *                       (parents can't be removed while DASH children
     *                       reference them) but non-OID attribute payloads
     *                       are not deep-copied.
     * NONE                - Meta is a passthrough for DASH objects
     *
     * Policy is currently hardcoded to EXISTENCE_REFCOUNT.
     */
    enum class DashCacheMode
    {
        FULL,
        EXISTENCE_REFCOUNT,
        NONE,
    };

    DashCacheMode getDashCacheMode();

    bool isDashObjectType(sai_object_type_t ot);

    /**
     * @brief Whether Meta should skip all pre/post validation and
     *        bookkeeping for this object type.
     *
    * True only when the object type is a DASH object type AND the
    * configured DashCacheMode is NONE.
     */
    inline bool bypassValidation(sai_object_type_t ot)
    {
        return isDashObjectType(ot)
            && getDashCacheMode() == DashCacheMode::NONE;
    }

    /**
     * @brief Whether Meta should deep-copy this attribute into
     *        m_saiObjectCollection for the given (object_type, attr).
     *
     * Always true for non-DASH object types.
     * For DASH object types it depends on the mode (see DashCacheMode).
     *
     * NOTE: callers should already short-circuit on DashCacheMode::NONE
     * before invoking the post-validation paths; this helper is only used
     * to gate the per-attribute deep copy inside post_create/post_set.
     */
    inline bool shouldCacheAttribute(
            sai_object_type_t ot,
            const sai_attr_metadata_t& md)
    {
        if (!isDashObjectType(ot))
        {
            return true;
        }
        switch (getDashCacheMode())
        {
            case DashCacheMode::FULL:               return true;
            case DashCacheMode::EXISTENCE_REFCOUNT: return md.isoidattribute;
            case DashCacheMode::NONE:               return false;
        }
        return true;
    }
}
