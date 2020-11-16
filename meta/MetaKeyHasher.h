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
                const sai_object_meta_key_t& a,
                const sai_object_meta_key_t& b) const;
    };
}
