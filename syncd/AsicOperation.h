#pragma once

extern "C"{
#include "sai.h"
}

#include "swss/table.h"

#include <memory>
#include <string>

namespace syncd
{
    class AsicOperation
    {
        public:

            AsicOperation(
                    _In_ int id, 
                    _In_ sai_object_id_t vid,
                    _In_ bool remove,
                    _In_ std::shared_ptr<swss::KeyOpFieldsValuesTuple>& operation);

            virtual ~AsicOperation() = default;

        public:

            int m_opId;

            sai_object_id_t m_vid;

            bool m_isRemove;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> m_op;

            std::string m_currentValue;
    };
}
