#pragma once

extern "C" {
#include "otai.h"
}

namespace syncd
{
    class VidManager
    {

        private:

            VidManager() = delete;

            ~VidManager() = delete;

        public:

            /**
             * @brief Switch id query.
             *
             * Return switch object id for given object if. If object type is
             * switch, it will return input value.
             *
             * For SAI_NULL_OBJECT_ID returns SAI_NULL_OBJECT_ID.
             *
             * Throws for invalid object ID.
             */
            static otai_object_id_t switchIdQuery(
                    _In_ otai_object_id_t objectId);

            /**
             * @brief Object type query.
             *
             * Returns object type for input object id. If object id is invalid
             * then returns SAI_OBJECT_TYPE_NULL.
             *
             * For SAI_NULL_OBJECT_ID returns SAI_OBJECT_TYPE_NULL.
             *
             * Throws for invalid object ID.
             */
            static otai_object_type_t objectTypeQuery(
                    _In_ otai_object_id_t objectId);

            /**
             * @brief Get switch index.
             *
             * Index range is <0..255>.
             *
             * Returns switch index for given oid.
             *
             * For SAI_NULL_OBJECT_ID returns 0.
             *
             * Throws for invalid object ID.
             */
            static uint32_t getSwitchIndex(
                    _In_ otai_object_id_t objectId);

            /**
             * @brief Get global context ID.
             *
             * Context range is <0..255>.
             *
             * Returns switch index for given oid.
             *
             * For SAI_NULL_OBJECT_ID returns 0.
             *
             * Throws for invalid object ID.
             */
            static uint32_t getGlobalContext(
                    _In_ otai_object_id_t objectId);

            /**
             * @brief Get object index.
             *
             * Returns object index.
             */
            static uint64_t getObjectIndex(
                    _In_ otai_object_id_t objectId);

            /**
             * @brief Update object index.
             *
             * Returns objects with updated object index.
             */
            static otai_object_id_t updateObjectIndex(
                    _In_ otai_object_id_t objectId,
                    _In_ uint64_t objectIndex);
    };
}
