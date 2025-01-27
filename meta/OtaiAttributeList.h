#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include "swss/table.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace otaimeta
{
    class OtaiAttributeList
    {
        public:

            OtaiAttributeList(
                    _In_ const otai_object_type_t object_type,
                    _In_ const std::vector<swss::FieldValueTuple> &values,
                    _In_ bool countOnly);

            OtaiAttributeList(
                    _In_ const otai_object_type_t object_type,
                    _In_ const std::unordered_map<std::string, std::string>& hash,
                    _In_ bool countOnly);

            virtual ~OtaiAttributeList();

        public:

            otai_attribute_t* get_attr_list();

            uint32_t get_attr_count();

            static std::vector<swss::FieldValueTuple> serialize_attr_list(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t attr_count,
                    _In_ const otai_attribute_t *attr_list,
                    _In_ bool countOnly);

        private:

            OtaiAttributeList(const OtaiAttributeList&);
            OtaiAttributeList& operator=(const OtaiAttributeList&);

            std::vector<otai_attribute_t> m_attr_list;
            std::vector<otai_attr_value_type_t> m_attr_value_type_list;
    };
}
