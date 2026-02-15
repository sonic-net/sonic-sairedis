#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "swss/table.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace saimeta
{
    class SaiAttributeList
    {
        public:

            SaiAttributeList(
                    _In_ const sai_object_type_t object_type,
                    _In_ const std::vector<swss::FieldValueTuple> &values,
                    _In_ bool countOnly);

            SaiAttributeList(
                    _In_ const sai_object_type_t object_type,
                    _In_ const std::unordered_map<std::string, std::string>& hash,
                    _In_ bool countOnly);

            virtual ~SaiAttributeList();

        public:

            sai_attribute_t* get_attr_list();

            uint32_t get_attr_count();

            static std::vector<swss::FieldValueTuple> serialize_attr_list(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ bool countOnly);

        private:

            SaiAttributeList(const SaiAttributeList&);
            SaiAttributeList& operator=(const SaiAttributeList&);

            std::vector<sai_attribute_t> m_attr_list;
            std::vector<sai_attr_value_type_t> m_attr_value_type_list;
    };
}
