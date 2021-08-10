#pragma once

#include "DummySaiInterface.h"

#include "lib/inc/VirtualObjectIdManager.h"

#include <memory>

namespace saimeta
{
    class MetaTestSaiInterface:
        public DummySaiInterface
    {
        public:

            MetaTestSaiInterface();

            virtual ~MetaTestSaiInterface() = default;

        public:

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

        public:

            virtual sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId) override;

        private:

            std::shared_ptr<sairedis::VirtualObjectIdManager> m_virtualObjectIdManager;
    };
}
