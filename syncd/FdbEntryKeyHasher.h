#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "swss/sal.h"

#include <cstddef>

namespace syncd
{
    struct FdbEntryKeyHasher
    {
        std::size_t operator()(
                _In_ const sai_fdb_entry_t& fe) const;

        bool operator()(
                _In_ const sai_fdb_entry_t& a,
                _In_ const sai_fdb_entry_t& b) const;
    };
}
