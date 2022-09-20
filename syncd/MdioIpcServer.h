#pragma once

#include <thread>
#include "VendorSai.h"

extern "C" {
#include <sai.h>
#include <saiversion.h>
}

namespace syncd
{
    class MdioIpcServer
    {
        public:

            MdioIpcServer(
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ int globalContext);

            virtual ~MdioIpcServer();

        public:

            void setSwitchId(
                    _In_ sai_object_id_t switchRid);

            int startMdioThread();

            void stopMdioThread();

        public:

            static void syncd_ipc_task_enter(void*);

        private:

            int syncd_ipc_task_main();

            sai_status_t syncd_ipc_cmd_mdio_common(char *resp, int argc, char *argv[]);

#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
            sai_status_t syncd_ipc_cmd_mdio_common_cl22(char *resp, int argc, char *argv[]);
#endif

            sai_status_t syncd_ipc_cmd_mdio(char *resp, int argc, char *argv[]);

            sai_status_t syncd_ipc_cmd_mdio_cl22(char *resp, int argc, char *argv[]);

            static bool m_syncdContext;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            sai_object_id_t m_switchRid;

            std::thread m_taskThread;

            int m_taskAlive;
    };
}
