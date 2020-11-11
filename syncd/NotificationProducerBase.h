#pragma once

#include <string>

#include "swss/table.h"
#include "swss/sal.h"

namespace syncd
{
    class NotificationProducerBase
    {
        public:

            NotificationProducerBase() = default;

            virtual ~NotificationProducerBase() = default;

        public:

            virtual void send(
                    _In_ const std::string& op, 
                    _In_ const std::string& data, 
                    _In_ const std::vector<swss::FieldValueTuple>& values) = 0;
    };
}
