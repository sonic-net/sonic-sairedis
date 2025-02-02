#pragma once

#include "meta/SaiOptions.h"

namespace syncd
{
    class VendorSaiOptions:
        public sairedis::SaiOptions
    {
        public:
            static constexpr const char *OPTIONS_KEY = "vok";

        public:

            bool m_checkAttrVersion = false;
    };
}
