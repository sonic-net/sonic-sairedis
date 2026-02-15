#pragma once

#include "SaiAttrWrapper.h"
#include "SaiObject.h"
#include "MetaKeyHasher.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace saimeta
{
    class SaiObjectCollection
    {
        public:

            SaiObjectCollection() = default;
            virtual ~SaiObjectCollection() = default;

        private:

            SaiObjectCollection(const SaiObjectCollection&) = delete;
            SaiObjectCollection& operator=(const SaiObjectCollection&) = delete;

        public:

            void clear();

            bool objectExists(
                    _In_ const sai_object_meta_key_t& metaKey) const;

            void createObject(
                    _In_ const sai_object_meta_key_t& metaKey);

            void removeObject(
                    _In_ const sai_object_meta_key_t& metaKey);

            void setObjectAttr(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const sai_attr_metadata_t& md,
                    _In_ const sai_attribute_t *attr);

            std::shared_ptr<SaiAttrWrapper> getObjectAttr(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ sai_attr_id_t id);

            std::vector<std::shared_ptr<SaiAttrWrapper>> getObjectAttributes(
                    _In_ const sai_object_meta_key_t& metaKey) const;

            std::vector<std::shared_ptr<SaiObject>> getObjectsByObjectType(
                    _In_ sai_object_type_t objectType);

            std::shared_ptr<SaiObject> getObject(
                    _In_ const sai_object_meta_key_t& metaKey) const;

            std::vector<sai_object_meta_key_t> getAllKeys() const;

        private:

            std::unordered_map<sai_object_meta_key_t, std::shared_ptr<SaiObject>, MetaKeyHasher, MetaKeyHasher> m_objects;

    };
}
