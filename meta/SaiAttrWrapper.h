#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "swss/sal.h"

namespace saimeta
{
    class SaiAttrWrapper
    {
        public:

            SaiAttrWrapper(
                    _In_ const sai_attr_metadata_t* meta,
                    _In_ const sai_attribute_t& attr);

            virtual ~SaiAttrWrapper();

        public:

            const sai_attribute_t* getSaiAttr() const;

            const sai_attr_metadata_t* getSaiAttrMetadata() const;

            sai_attr_id_t getAttrId() const;

        private:

            SaiAttrWrapper(const SaiAttrWrapper&) = delete;
            SaiAttrWrapper& operator=(const SaiAttrWrapper&) = delete;

        private:

            /**
             * @brief Attribute metadata associated with given attribute.
             */
            const sai_attr_metadata_t* m_meta;

            sai_attribute_t m_attr;
    };
}
