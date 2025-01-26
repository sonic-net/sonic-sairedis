#pragma once

extern "C" {
#include "otai.h"
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
                    _In_ otai_object_id_t switchRid);
    };
}
