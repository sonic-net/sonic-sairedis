#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include <string>
#include <vector>

namespace syncd
{
    /**
     * @brief Class represents single attribute
     *
     * Some attributes are lists, and have allocated memory, this class will help to
     * handle memory management and also will keep metadata for attribute.
     */
    class OtaiAttr
    {
        private:

            /*
             * Copy constructor and assignment operator are marked as private to
             * prevent copy of this object, since attribute can contain pointers to
             * list which can lead to double free when object copy.
             *
             * This can be solved by making proper implementations of those
             * methods, currently this is not required.
             */

            OtaiAttr(const OtaiAttr&) = delete;
            OtaiAttr& operator=(const OtaiAttr&) = delete;

        public:

            /**
             * @brief Constructor
             *
             * @param[in] str_attr_id Attribute is as string
             * @param[in] str_attr_value Attribute value as string
             */
            OtaiAttr(
                    _In_ const std::string &str_attr_id,
                    _In_ const std::string &str_attr_value);

            virtual ~OtaiAttr();

        public:

            otai_attribute_t* getRWOtaiAttr();

            const otai_attribute_t* getOtaiAttr() const;

            /**
             * @brief Gets value.oid of attribute value.
             *
             * If attribute is not OID exception will be thrown.
             *
             * @return Oid field of attribute value.
             */
            otai_object_id_t getOid() const;

            /**
             * @brief Tells whether attribute contains OIDs
             *
             * @return True if attribute contains OIDs, false otherwise
             */
            bool isObjectIdAttr() const;

                const std::string& getStrAttrId() const;

            const std::string& getStrAttrValue() const;

            const otai_attr_metadata_t* getAttrMetadata() const;

            void updateValue();

            /**
             * @brief Get OID list from attribute
             *
             * Based on serialization type attribute may be oid attribute, oid list
             * attribute or non oid attribute. This method will extract all those oids from
             * this attribute and return as vector.  This is handy when we need processing
             * oids per attribute.
             *
             * @return Object list used in attribute
             */
            std::vector<otai_object_id_t> getOidListFromAttribute() const;

        private:

            const std::string m_str_attr_id;

            std::string m_str_attr_value;

            const otai_attr_metadata_t* m_meta;

            otai_attribute_t m_attr;
    };
}
