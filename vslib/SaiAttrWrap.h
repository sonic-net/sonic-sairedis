#pragma once

extern "C" {
#include "saimetadata.h"
}

#include <string>

namespace saivs
{
    // TODO unify wrapper and add to common
    class SaiAttrWrap
    {
        private:

            SaiAttrWrap(const SaiAttrWrap&) = delete;
            SaiAttrWrap& operator=(const SaiAttrWrap&) = delete;

        public:

            SaiAttrWrap(
                    _In_ sai_object_type_t object_type,
                    _In_ const sai_attribute_t *attr);

            SaiAttrWrap(
                    _In_ const std::string& attrId,
                    _In_ const std::string& attrValue);

            virtual ~SaiAttrWrap();

        public:

            const sai_attribute_t* getAttr() const;

            const sai_attr_metadata_t* getAttrMetadata() const;

            const std::string& getAttrStrValue() const;

        private:

            const sai_attr_metadata_t *m_meta;

            sai_attribute_t m_attr;

            std::string m_value;
    };
}
