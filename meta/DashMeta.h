#pragma once

extern "C" {
#include "saimetadata.h"
}

namespace saimeta
{
    /**
     * @brief Whether the given object type is a DASH type subject to the
     *        DASH meta-cache policy.
     */
    bool isDashObjectType(sai_object_type_t ot);

    /**
     * @brief Whether Meta should skip all pre/post validation and
     *        bookkeeping for this object type.
     */
    bool bypassValidation(sai_object_type_t ot);

    /**
     * @brief Whether Meta should deep-copy this attribute into
     *        m_saiObjectCollection for the given (object_type, attr).
     */
    bool shouldCacheAttribute(
            sai_object_type_t ot,
            const sai_attr_metadata_t& md);

    /**
     * @brief Human-readable name of the currently configured DASH
     *        meta-cache mode (e.g. "FULL", "EXISTENCE_REFCOUNT", "NONE").
     */
    const char* dashCacheModeName();
}
