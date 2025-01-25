#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include "swss/sal.h"

namespace otaimeta
{
    class OtaiAttrWrapper
    {
        public:

            OtaiAttrWrapper(
                    _In_ const otai_attr_metadata_t* meta,
                    _In_ const otai_attribute_t& attr);

            virtual ~OtaiAttrWrapper();

        public:

            const otai_attribute_t* getOtaiAttr() const;

            const otai_attr_metadata_t* getOtaiAttrMetadata() const;

            otai_attr_id_t getAttrId() const;

        private:

            OtaiAttrWrapper(const OtaiAttrWrapper&) = delete;
            OtaiAttrWrapper& operator=(const OtaiAttrWrapper&) = delete;

        private:

            /**
             * @brief Attribute metadata associated with given attribute.
             */
            const otai_attr_metadata_t* m_meta;

            otai_attribute_t m_attr;
    };
}
