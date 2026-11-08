#pragma once

#include "Switch.h"

#include <memory>
#include <map>

namespace sairedis
{
    class SwitchContainer
    {
        public:

            SwitchContainer() = default;

            virtual ~SwitchContainer() = default;

        public:

            /**
             * @brief Insert switch to container.
             *
             * Throws when switch already exists in container.
             */
            void insert(
                    _In_ std::shared_ptr<Switch> sw);

            /**
             * @brief Remove switch from container.
             *
             * Throws when switch is not present in container.
             */
            void removeSwitch(
                    _In_ sai_object_id_t switchId);

            /**
             * @brief Remove switch from container.
             *
             * Throws when switch is not present in container.
             */
            void removeSwitch(
                    _In_ std::shared_ptr<Switch> sw);

            /**
             * @brief Get switch from the container.
             *
             * If switch is not present in container returns NULL pointer.
             */
            std::shared_ptr<Switch> getSwitch(
                    _In_ sai_object_id_t switchId) const;

            /**
             * @brief Removes all switches from container.
             */
            void clear();

            /**
             * @brief Check whether switch is in the container.
             */
            bool contains(
                    _In_ sai_object_id_t switchId) const;

            /**
             * @brief Get switch by hardware info.
             *
             * Container allows only one switch with specific hardware info.
             */
            std::shared_ptr<Switch> getSwitchByHardwareInfo(
                    _In_ const std::string& hardwareInfo) const;

        private:

            std::map<sai_object_id_t, std::shared_ptr<Switch>> m_switchMap;

    };
}
