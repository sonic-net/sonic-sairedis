#include <string.h>
#include <errno.h>
#include <cstdarg>
#include <cassert>

#include "Ipc.h"
#include "Mdio.h"

using namespace swss;
using namespace syncd;

void RpcMdio::process(Ipc *ipc, const IpcMsg *msg, struct sockaddr *peer, socklen_t addrlen)
{
    if (msg->subtype != IPC_SUBTYPE_MDIO)
    {
        SWSS_LOG_NOTICE("mdio rpc process: type %u is not mine", msg->subtype);
        return;
    }

    if (msg->type == IPC_TYPE_REQ)
    {
        const IpcServer *ipcServer = dynamic_cast<const IpcServer*>(ipc);
        sai_object_id_t switch_id = ipcServer->getSwitchId();

        if (switch_id == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_NOTICE("mdio rpc process: switch id null");
            return;
        }

        union {
            IpcMsg msg;
            char buf[IPC_MSG_MAX_LEN];
        } m;
        IpcMsg *ipcMsg = &m.msg;
        IpcMsgMdioReply *mdioReply = (IpcMsgMdioReply*)(ipcMsg + 1);

        const IpcMsgMdioReq *mdioReq = (const IpcMsgMdioReq*)(msg + 1);
        auto switch_api = ipcServer->getSwitchApi();
        sai_status_t status = switch_api->switch_mdio_read(
            switch_id,
            mdioReq->mdio_addr,
            mdioReq->reg_addr,
            mdioReq->reg_num,
            mdioReply->reg_data);

        ipcMsg->type = IPC_TYPE_RESP;
        ipcMsg->subtype = IPC_SUBTYPE_MDIO;
        ipcMsg->length = sizeof(IpcMsg);

        mdioReply->status = status;
        mdioReply->reg_num = 0;
        ipcMsg->length += (uint32_t)sizeof(*mdioReply);
        if (status == SAI_STATUS_SUCCESS)
        {
            mdioReply->reg_num = mdioReq->reg_num;
            ipcMsg->length += mdioReply->reg_num * (uint32_t)sizeof(mdioReply->reg_data[0]);
        }

        ipc->sendto(ipcMsg, peer, addrlen);
    }
    else if (msg->type == IPC_TYPE_NOTIF)
    {
        // Todo: Not support now
    }
}

int RpcMdio::call(IpcClient *client...)
{
    std::va_list args;
    va_start(args, client);
    uint32_t cmd = va_arg(args, uint32_t);
    if (cmd < MDIO_CMD_MIN || cmd >= MDIO_CMD_MAX)
    {
        SWSS_LOG_NOTICE("mdio call: unknown command %u", cmd);
        va_end(args);
        return SAI_STATUS_FAILURE;
    }
    uint32_t device_addr = va_arg(args, uint32_t);
    uint32_t start_reg_addr = va_arg(args, uint32_t);
    uint32_t number_of_registers = va_arg(args, uint32_t);
    uint32_t *reg_val = va_arg(args, uint32_t*);
    va_end(args);

    union {
        IpcMsg msg;
        char buf[IPC_MSG_MAX_LEN];
    } m;
    IpcMsg *ipcMsg = &m.msg;
    IpcMsgMdioReq *mdioReq = (IpcMsgMdioReq*)(ipcMsg + 1);

    ipcMsg->type = IPC_TYPE_REQ;
    ipcMsg->subtype = IPC_SUBTYPE_MDIO;
    mdioReq->cmd = cmd;
    mdioReq->mdio_addr = device_addr;
    mdioReq->reg_addr = start_reg_addr;
    mdioReq->reg_num = number_of_registers;
    ipcMsg->length = sizeof(*ipcMsg) + sizeof(*mdioReq);

    if (cmd == MDIO_WRITE_CL22 || cmd == MDIO_WRITE_CL45)
    {
        uint32_t *reg_data = mdioReq->reg_data;
        for (uint32_t i = 0; i < mdioReq->reg_num; i++)
        {
            reg_data[i] = reg_val[i];
        }
        ipcMsg->length += mdioReq->reg_num * (uint32_t)sizeof(mdioReq->reg_data[0]);
    }

    if (client->send(ipcMsg) < 0)
    {
        SWSS_LOG_NOTICE("mdio call: send request message failure: %s", strerror(errno));
        return SAI_STATUS_FAILURE;
    }

    if (client->recv(ipcMsg, IPC_MSG_MAX_LEN) > 0)
    {
        assert(ipcMsg->type == IPC_TYPE_RESP);
        assert(ipcMsg->length > sizeof(*ipcMsg));

        IpcMsgMdioReply *mdioReply = (IpcMsgMdioReply*)(ipcMsg + 1);
        if (mdioReply->status == SAI_STATUS_SUCCESS)
        {
            uint32_t *reg_data = mdioReply->reg_data;
            for (uint32_t i = 0; i < mdioReply->reg_num; i++)
                reg_val[i] = reg_data[i];
        }

        return mdioReply->status;
    }
    else
    {
        SWSS_LOG_NOTICE("mdio call: recv response failure: %s", strerror(errno));
        return SAI_STATUS_FAILURE;
    }
}

static sai_status_t switch_register_mdio_cmd(
        _In_ uint64_t platform_context,
        _In_ uint32_t cmd,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Inout_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<IpcClient> client = std::make_shared<IpcClient>();
    auto stub = client->rpcAt(IPC_SUBTYPE_MDIO);

    return stub->call(client.get(), cmd, device_addr, start_reg_addr, number_of_registers, reg_val);
}

sai_status_t switch_register_mdio_cl22_read(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return switch_register_mdio_cmd(platform_context, MDIO_READ_CL22,
        device_addr, start_reg_addr, number_of_registers, reg_val);
}

sai_status_t switch_register_mdio_cl22_write(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return switch_register_mdio_cmd(platform_context, MDIO_WRITE_CL22,
        device_addr, start_reg_addr, number_of_registers, reg_val);
}

sai_status_t switch_register_mdio_cl45_read(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return switch_register_mdio_cmd(platform_context, MDIO_READ_CL45,
        device_addr, start_reg_addr, number_of_registers, reg_val);
}

sai_status_t switch_register_mdio_cl45_write(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return switch_register_mdio_cmd(platform_context, MDIO_WRITE_CL45,
        device_addr, start_reg_addr, number_of_registers, reg_val);
}
