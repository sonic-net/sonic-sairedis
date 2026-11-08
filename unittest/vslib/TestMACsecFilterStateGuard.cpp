#include "MACsecFilterStateGuard.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(MACsecFilterStateGuard, ctr)
{
    MACsecFilter::MACsecFilterState state = MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_IDLE;

    {
        MACsecFilterStateGuard guard(state, MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_BUSY);

        EXPECT_EQ(state, MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_BUSY);
    }

    EXPECT_EQ(state, MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_IDLE);
}
