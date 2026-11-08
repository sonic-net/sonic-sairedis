#pragma once

extern "C" {
#include "sai.h"
}

#include "SwitchState.h"
#include "FdbInfo.h"

#include <set>

namespace saivs
{
    class WarmBootState
    {
        public:

            sai_object_id_t m_switchId;

            std::set<FdbInfo> m_fdbInfoSet;

            SwitchState::ObjectHash m_objectHash;
    };
}
