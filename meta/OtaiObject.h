#pragma once

#include "OtaiAttrWrapper.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace otaimeta
{
    class OtaiObject
    {
        public:

            OtaiObject(
                    _In_ const otai_object_meta_key_t& metaKey);

            virtual ~OtaiObject() = default;

        private:

            OtaiObject(const OtaiObject&) = delete;
            OtaiObject& operator=(const OtaiObject&) = delete;

        public:

            otai_object_type_t getObjectType() const;

            bool hasAttr(
                    _In_ otai_attr_id_t id) const;

            const otai_object_meta_key_t& getMetaKey() const;

            void setAttr(
                    _In_ const otai_attr_metadata_t* md,
                    _In_ const otai_attribute_t *attr);

            void setAttr(
                    _In_ std::shared_ptr<OtaiAttrWrapper> attr);

            std::shared_ptr<OtaiAttrWrapper> getAttr(
                    _In_ otai_attr_id_t id) const;

            std::vector<std::shared_ptr<OtaiAttrWrapper>> getAttributes() const;

        private:

            otai_object_meta_key_t m_metaKey;

            std::unordered_map<otai_attr_id_t, std::shared_ptr<OtaiAttrWrapper>> m_attrs;
    };
}
