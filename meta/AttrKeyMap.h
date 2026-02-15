#pragma once

extern "C" {
#include "saimetadata.h"
}

#include <string>
#include <vector>
#include <unordered_map>

namespace saimeta
{
    class AttrKeyMap
    {
        public:

            AttrKeyMap() = default;

            virtual ~AttrKeyMap() = default;

        public:

            void clear();

            bool attrKeyExists(
                    _In_ const std::string& attrKey) const;

            void insert(
                    _In_ const std::string& metaKey,
                    _In_ const std::string& attrKey);

            void eraseMetaKey(
                    _In_ const std::string& metaKey);

            /**
             * @brief Construct key based on attributes marked as keys.
             */
            static std::string constructKey(
                    _In_ sai_object_id_t switchId,
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t* attrList);

            std::vector<std::string> getAllKeys() const;

        private:

            /**
             * @brief map holding attribute keys.
             *
             * Key is serialized meta key.
             *
             * Value is constructed key from attributes.
             *
             * Map must contain meta Key and attr Key, since when we are removing
             * object, we only have meta Key, and we can't construct attr Key (we
             * could since we have local db, but this way is safer).
             */
            std::unordered_map<std::string, std::string> m_map;
    };
}
