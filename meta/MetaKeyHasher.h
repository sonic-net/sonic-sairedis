#pragma once

extern "C" {
#include "saimetadata.h"
}

#include <string>

namespace saimeta
{
    struct MetaKeyHasher
    {
        std::size_t operator()(
                _In_ const sai_object_meta_key_t& k) const;

        bool operator()(
                _In_ const sai_object_meta_key_t& a,
                _In_ const sai_object_meta_key_t& b) const;
    };
}
