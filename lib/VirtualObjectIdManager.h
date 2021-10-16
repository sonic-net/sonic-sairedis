#pragma once

extern "C" {
#include "sai.h"
}

#include "OidIndexGenerator.h"
#include "SwitchConfigContainer.h"

#include <set>
#include <memory>

namespace sairedis
{
    class VirtualObjectIdManager
    {

        public:

            VirtualObjectIdManager(
                    _In_ uint32_t globalContext,
                    _In_ std::shared_ptr<SwitchConfigContainer> scc,
                    _In_ std::shared_ptr<OidIndexGenerator> oidIndexGenerator);

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
             * @brief Allocate new switch object id.
             */
            sai_object_id_t allocateNewSwitchObjectId(
                    _In_ const std::string& hardwareInfo);

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
             * @brief Construct object id.
             *
             * Using all input parameters to construct object id.
             */
            static sai_object_id_t constructObjectId(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t switchIndex,
                    _In_ uint64_t objectIndex,
                    _In_ uint32_t globalContext);

        public:

            /**
             * @brief Switch id query.
             *
             * Return switch object id for given object if. If object type is
             * switch, it will return input value.
             *
             * Return SAI_NULL_OBJECT_ID if given object id has invalid object type.
             */
            static sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId);

            /**
             * @brief Object type query.
             *
             * Returns object type for input object id. If object id is invalid
             * then returns SAI_OBJECT_TYPE_NULL.
             */
            static sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId);

            /**
             * @brief Get switch index.
             *
             * Index range is <0..255>.
             *
             * Returns switch index for given oid. If oid is invalid, returns 0.
             */
            static uint32_t getSwitchIndex(
                    _In_ sai_object_id_t objectId);

            /**
             * @brief Get global context.
             *
             * Index range is <0..255>.
             *
             * Returns global context for given oid. If oid is invalid, returns 0.
             */
            static uint32_t getGlobalContext(
                    _In_ sai_object_id_t objectId);

            /**
             * @brief Get object index.
             *
             * Returns object index.
             */
            static uint64_t getObjectIndex(
                    _In_ sai_object_id_t objectId);

            /**
             * @brief Update object index.
             *
             * Returns objects with updated object index.
             */
            static sai_object_id_t updateObjectIndex(
                    _In_ sai_object_id_t objectId,
                    _In_ uint64_t objectIndex);

        private:

            /**
             * @brief Global context value.
             *
             * Will be encoded in every object id, and it will point to global
             * (system wide) syncd instance.
             */
            uint32_t m_globalContext;

            std::shared_ptr<SwitchConfigContainer> m_container;

            /**
             * @brief Oid index generator.
             */
            std::shared_ptr<OidIndexGenerator> m_oidIndexGenerator;

            /**
             * @brief Set of allocated switch indexes.
             */
            std::set<uint32_t> m_switchIndexes;
    };
}
