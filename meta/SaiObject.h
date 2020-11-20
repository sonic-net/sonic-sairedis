#pragma once

#include "SaiAttrWrapper.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace saimeta
{
    class SaiObject
    {
        public:

            SaiObject(
                    _In_ const sai_object_meta_key_t& metaKey);

            virtual ~SaiObject() = default;

        private:

            SaiObject(const SaiObject&) = delete;
            SaiObject& operator=(const SaiObject&) = delete;

        public:

            sai_object_type_t getObjectType() const;

            bool hasAttr(
                    _In_ sai_attr_id_t id) const;

            const sai_object_meta_key_t& getMetaKey() const;

            void setAttr(
                    _In_ const sai_attr_metadata_t* md,
                    _In_ const sai_attribute_t *attr);

            void setAttr(
                    _In_ std::shared_ptr<SaiAttrWrapper> attr);

            std::shared_ptr<SaiAttrWrapper> getAttr(
                    _In_ sai_attr_id_t id) const;

            std::vector<std::shared_ptr<SaiAttrWrapper>> getAttributes() const;

        private:

            sai_object_meta_key_t m_metaKey;

            std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttrWrapper>> m_attrs;
    };
}
