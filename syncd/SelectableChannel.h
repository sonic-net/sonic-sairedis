#pragma once

#include "swss/selectable.h"
#include "swss/table.h"
#include "swss/sal.h"

#include <string>
#include <vector>

namespace syncd
{
    class SelectableChannel:
        public swss::Selectable
    {
        public:

            SelectableChannel(
                    _In_ int pri = 0);

            virtual ~SelectableChannel() = default;

        public:

            virtual bool empty() = 0;

            virtual void pop(
                    _Out_ swss::KeyOpFieldsValuesTuple& kco,
                    _In_ bool initViewMode) = 0;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& op) = 0;
    };
}
