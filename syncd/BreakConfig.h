#pragma once

extern "C"{
#include "sai.h"
}

#include <set>

namespace syncd
{
    class BreakConfig
    {
        public:

            BreakConfig() = default;

            ~BreakConfig() = default;

        public:

            void insert(
                    _In_ sai_object_type_t objectType);

            void remove(
                    _In_ sai_object_type_t objectType);

            void clear();

            bool shouldBreakBeforeMake(
                    _In_ sai_object_type_t objectType) const;

            size_t size() const;

        private:

            std::set<sai_object_type_t> m_set;

    };
}
