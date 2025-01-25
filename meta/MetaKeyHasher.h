#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include <string>

namespace otaimeta
{
    struct MetaKeyHasher
    {
        std::size_t operator()(
                _In_ const otai_object_meta_key_t& k) const;

        bool operator()(
                _In_ const otai_object_meta_key_t& a,
                _In_ const otai_object_meta_key_t& b) const;
    };
}
