#pragma once

extern "C" {
#include <sai.h>
}

#include <set>

namespace sairedis
{
    class VirtualObjectIdManager
    {

        public:

            VirtualObjectIdManager(
                    _In_ uint32_t globalContext);

            virtual ~VirtualObjectIdManager() = default;

        public:

            /**
             * @brief Switch id query.
             *
             * Return switch object id for given object if. If object type is
             * switch, it will return input value.
             *
             * Throws if given object id has invalid object type.
             *
             * For SAI_NULL_OBJECT_ID input will return SAI_NULL_OBJECT_ID.
             */
            sai_object_id_t saiSwitchIdQuery(
                    _In_ sai_object_id_t objectId) const;

            /**
             * @brief Object type query.
             *
             * Returns object type for input object id. If object id is invalid
             * then returns SAI_OBJECT_TYPE_NULL.
             */
            sai_object_type_t saiObjectTypeQuery(
                    _In_ sai_object_id_t objectId) const;

            /**
             * @brief Clear switch index set. 
             *
             * New switch index allocation will start from the beginning.
             */
            void clear();

            /**
             * @brief Allocate new object id on a given switch.
             *
             * If object type is switch, then switch id param is ignored.
             *
             * Throws when object type is switch and there are no more
             * available switch indexes.
             */
            sai_object_id_t allocateNewObjectId(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t switchId);
            /**
             * @brief release allocated object id.
             *
             * If object type is switch, then switch index will be released.
             */
            void releaseObjectId(
                    _In_ sai_object_id_t objectId);

        private:

            /**
             * @brief Release given switch index.
             *
             * Will throw if index is not allocated.
             */
            void releaseSwitchIndex(
                    _In_ uint32_t index);

            /**
             * @brief Allocate new switch index.
             *
             * Will throw if there are no more available switch indexes.
             */
            uint32_t allocateNewSwitchIndex();

            /**
             * @brief Construct object id.
             *
             * Using all input parameters to construct object id.
             */
            static sai_object_id_t constructObjectId(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t switchIndex,
                    _In_ uint64_t objectIndex,
                    _In_ uint32_t globalContext);

        private:

            /**
             * @brief Global context value.
             *
             * Will be encoded in every object id, and it will point to global
             * (system wide) syncd instance.
             */
            uint32_t m_globalContext;

            /**
             * @brief Set of allocated switch indexes.
             */
            std::set<uint32_t> m_switchIndexes;
    };
}
