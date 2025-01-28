#pragma once

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#include <set>
#include <string>

namespace syncd
{
    class AttrVersionChecker
    {
        public:

            AttrVersionChecker();

        public:

            void enable(
                    _In_ bool enable);

            void setSaiApiVersion(
                    _In_ sai_api_version_t version);

            void reset();

            bool isSufficientVersion(
                    _In_ const sai_attr_metadata_t *md);

        private:

            bool m_enabled;

            sai_api_version_t m_saiApiVersion;

            std::set<std::string> m_visitedAttributes;
    };
}
