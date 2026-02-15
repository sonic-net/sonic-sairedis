#pragma once

extern "C" {
#include "sai.h"
}

namespace syncd
{
    class GlobalSwitchId
    {
        private:

            GlobalSwitchId() = delete;
            ~GlobalSwitchId() = delete;

        public:

            static void setSwitchId(
                    _In_ sai_object_id_t switchRid);
    };
}
