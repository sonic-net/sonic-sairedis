#pragma once

#include "OtaiAttrWrapper.h"
#include "OtaiObject.h"
#include "MetaKeyHasher.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace otaimeta
{
    class OtaiObjectCollection
    {
        public:

            OtaiObjectCollection() = default;
            virtual ~OtaiObjectCollection() = default;

        private:

            OtaiObjectCollection(const OtaiObjectCollection&) = delete;
            OtaiObjectCollection& operator=(const OtaiObjectCollection&) = delete;

        public:

            void clear();

            bool objectExists(
                    _In_ const otai_object_meta_key_t& metaKey) const;

            void createObject(
                    _In_ const otai_object_meta_key_t& metaKey);

            void removeObject(
                    _In_ const otai_object_meta_key_t& metaKey);

            void setObjectAttr(
                    _In_ const otai_object_meta_key_t& metaKey,
                    _In_ const otai_attr_metadata_t& md,
                    _In_ const otai_attribute_t *attr);

            std::shared_ptr<OtaiAttrWrapper> getObjectAttr(
                    _In_ const otai_object_meta_key_t& metaKey,
                    _In_ otai_attr_id_t id);

            std::vector<std::shared_ptr<OtaiAttrWrapper>> getObjectAttributes(
                    _In_ const otai_object_meta_key_t& metaKey) const;

            std::vector<std::shared_ptr<OtaiObject>> getObjectsByObjectType(
                    _In_ otai_object_type_t objectType);

            std::shared_ptr<OtaiObject> getObject(
                    _In_ const otai_object_meta_key_t& metaKey) const;

            std::vector<otai_object_meta_key_t> getAllKeys() const;

        private:

            std::unordered_map<otai_object_meta_key_t, std::shared_ptr<OtaiObject>, MetaKeyHasher, MetaKeyHasher> m_objects;

    };
}
