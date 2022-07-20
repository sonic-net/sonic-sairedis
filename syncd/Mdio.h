#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>

#include "Ipc.h"

extern "C" {
#include "sai.h"

sai_status_t switch_register_mdio_cl22_read(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val);

sai_status_t switch_register_mdio_cl22_write(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ uint32_t *reg_val);


sai_status_t switch_register_mdio_cl45_read(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val);

sai_status_t switch_register_mdio_cl45_write(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ uint32_t *reg_val);

}

namespace syncd
{
    enum {
        MDIO_CMD_MIN = 1,
        MDIO_READ_CL22 = MDIO_CMD_MIN,
        MDIO_WRITE_CL22,
        MDIO_READ_CL45,
        MDIO_WRITE_CL45,
        MDIO_CMD_MAX,
    };

    struct IpcMsgMdioReq {
        uint32_t cmd;
        uint32_t mdio_addr;
        uint32_t reg_addr;
        uint32_t reg_num;
        uint32_t reg_data[0];
    };

    struct IpcMsgMdioReply {
        sai_status_t status;
        uint32_t reg_num;
        uint32_t reg_data[0];
    };

    class RpcMdio: public RpcStub {
        int call(IpcClient *client...) override;
        void process(Ipc *ipc, const IpcMsg *msg, struct sockaddr *peer, socklen_t addrlen) override;
    };
}
