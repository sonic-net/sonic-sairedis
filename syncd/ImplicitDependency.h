/*
 * SPDX-FileCopyrightText: Nexthop Systems Inc.
 * Copyright (c) 2026 Nexthop Systems Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#include <string>
#include <vector>

namespace syncd
{
    /**
     * @brief Creation dependencies that no attribute expresses.
     *
     * Replaying a view by following OID attributes creates every object after
     * the objects it references, but some objects also need another object
     * that they do not reference.
     *
     * Objects are matched by key: an object that needs others (dependant key)
     * must be created after, and removed before, every object that provides
     * the same key (provider key).
     *
     * A P2P VXLAN tunnel shares its local endpoint with the P2MP VXLAN tunnel
     * of the same source IP and with the P2MP VXLAN tunnel termination entry
     * whose destination IP is that source IP. A SAI may look up that endpoint,
     * set up by those objects, when creating the P2P tunnel and when removing
     * it.
     */
    class ImplicitDependency
    {
        private:

            ImplicitDependency() = delete;
            ~ImplicitDependency() = delete;

        public:

            /**
             * @brief Whether objects of this type can have any key.
             */
            static bool hasImplicitDependencies(
                    _In_ sai_object_type_t objectType);

            /**
             * @brief The object types that can have any key.
             */
            static const std::vector<sai_object_type_t>& getObjectTypes();

            /**
             * @brief Key under which this object is needed by others.
             *
             * @return Empty string when no object needs this one.
             */
            static std::string getProviderKey(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t* attrList);

            /**
             * @brief Key of the objects this object needs to exist while it does.
             *
             * @return Empty string when this object needs no other object.
             */
            static std::string getDependantKey(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t* attrList);
    };
}
