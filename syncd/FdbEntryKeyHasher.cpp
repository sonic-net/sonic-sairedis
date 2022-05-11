#include "FdbEntryKeyHasher.h"

#include "swss/logger.h"

#include <cstring>

using namespace syncd;

std::size_t FdbEntryKeyHasher::operator()(
        _In_ const sai_fdb_entry_t& fe) const
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    uint32_t data;

    // use low 4 bytes of mac address as hash value
    // use memcpy instead of cast because of strict-aliasing rules
    memcpy(&data, fe.mac_address + 2, sizeof(uint32_t));

    return data;
}

bool FdbEntryKeyHasher::operator()(
        _In_ const sai_fdb_entry_t& a,
        _In_ const sai_fdb_entry_t& b) const
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        memcmp(a.mac_address, b.mac_address, sizeof(a.mac_address)) == 0;
}
