#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unistd.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"
#include "swss/selectable.h"
#include "meta/SaiInterface.h"

namespace syncd
{
    class RpcStub;

    enum {
        IPC_TYPE_MIN = 0,
        IPC_TYPE_REQ = IPC_TYPE_MIN, // request
        IPC_TYPE_RESP,  // response
        IPC_TYPE_NOTIF, // notification
        IPC_TYPE_MAX,
    };

    enum {
        IPC_SUBTYPE_MIN = 0,
        IPC_SUBTYPE_MDIO = IPC_SUBTYPE_MIN, // mdio rpc
        IPC_SUBTYPE_MAX,
    };

    struct IpcMsg {
        uint16_t type;
        uint16_t subtype;
        uint32_t length;
        char data[0];
    };

    static constexpr uint32_t IPC_MSG_MIN_LEN = sizeof(IpcMsg);
    static constexpr uint32_t IPC_MSG_MAX_LEN = 1024;
    static constexpr uint32_t IPC_TIMEOUT_SEC = 2;    // in seconds
    static constexpr char IPC_SERVER[] = "/var/run/redis/syncd.sock";

    class Ipc: public swss::Selectable
    {
    public:

        Ipc(int pri = 0);

        virtual ~Ipc() {
            if (m_sock >= 0)
            {
                close(m_sock);
            }
        }

        std::shared_ptr<RpcStub> rpcAt(int rpc) const;
        void registerRpc(const std::shared_ptr<RpcStub> &stub);

        int sendto(const IpcMsg *msg, const struct sockaddr *dest_addr, socklen_t addrlen);
        int recvfrom(IpcMsg *msg, int size, struct sockaddr *src_addr, socklen_t *addrlen);
        int send(const IpcMsg *msg);
        int recv(IpcMsg *msg, int size);

    public: // Selectable method
        int getFd() override;
        uint64_t readData() override;
        bool hasData() override;
        bool hasCachedData() override;
        bool initializedWithData() override;
        void updateAfterRead() override;

    protected:
        int m_sock;

    private:
        std::unordered_map<int, std::shared_ptr<RpcStub>> m_rpcMap;
    };

    class IpcClient: public Ipc
    {
    public:
        IpcClient(const char *server_path = IPC_SERVER);

    private:
        int create(const char *server_path);
    };

    class IpcServer: public Ipc
    {
    public:
        IpcServer(const char *sock_path = IPC_SERVER);
        ~IpcServer();

        void startThread();
        void endThread();
        void threadProc();

        void setSwitchId(sai_object_id_t switchRid) {
            m_switch_rid = switchRid;
        }

        sai_object_id_t getSwitchId() const {
            return m_switch_rid;
        }

        void setSwitchApi(const sai_switch_api_t* api) {
            m_switch_api = api;
        }

        const sai_switch_api_t* getSwitchApi(void) const {
            return m_switch_api;
        }

    private:
        int create(const char *sock_path);
        void processMsg();

    private:
        std::shared_ptr<std::thread> m_thread;
        sai_object_id_t m_switch_rid;
        const sai_switch_api_t* m_switch_api;
    };

    class RpcStub {
    public:
        uint16_t rpc;
        virtual int call(IpcClient *client...) = 0;
        virtual void process(Ipc *ipc, const IpcMsg *msg, struct sockaddr *peer, socklen_t addrlen) = 0;
    };
}
