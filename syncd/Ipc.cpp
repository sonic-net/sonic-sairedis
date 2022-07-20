#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "swss/select.h"

#include "Ipc.h"
#include "Mdio.h"

using namespace swss;
using namespace syncd;


Ipc::Ipc(int pri)
    :Selectable(pri), m_sock(-1)
{
    SWSS_LOG_ENTER();

    registerRpc(std::make_shared<RpcMdio>());
}

std::shared_ptr<RpcStub> Ipc::rpcAt(int rpc) const {
    try {
        return m_rpcMap.at(rpc);
    }
    catch (const std::out_of_range&) {
        return nullptr;
    }
}

void Ipc::registerRpc(const std::shared_ptr<RpcStub> &stub) {
    m_rpcMap.insert({stub->rpc, stub});
}

int Ipc::getFd()
{
    SWSS_LOG_ENTER();

    return m_sock;
}

uint64_t Ipc::readData()
{
    SWSS_LOG_ENTER();

    // Do nothing now, need to actively call recv/recvfrom later.
    return 0;
}

bool Ipc::hasData()
{
    SWSS_LOG_ENTER();

    return true;
}

bool Ipc::hasCachedData()
{
    SWSS_LOG_ENTER();

    return false;
}

bool Ipc::initializedWithData()
{
    SWSS_LOG_ENTER();

    return false;
}

void Ipc::updateAfterRead()
{
    SWSS_LOG_ENTER();
}

int Ipc::sendto(const IpcMsg *msg, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return (int)::sendto(m_sock, msg, msg->length, 0, dest_addr, addrlen);
}

int Ipc::recvfrom(IpcMsg *msg, int size, struct sockaddr *src_addr, socklen_t *addrlen)
{
    return (int)::recvfrom(m_sock, msg, size, 0, src_addr, addrlen);
}

int Ipc::send(const IpcMsg *msg)
{
    return sendto(msg, NULL, 0);
}

int Ipc::recv(IpcMsg *msg, int size)
{
    return recvfrom(msg, size, NULL, NULL);
}

IpcClient::IpcClient(const char *server_path)
    :Ipc()
{
    SWSS_LOG_ENTER();

    m_sock = create(server_path);
}

int IpcClient::create(const char *server_path)
{
    SWSS_LOG_ENTER();

    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        SWSS_LOG_ERROR("Ipc client socket error: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un name;
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, server_path, sizeof(name.sun_path) - 1);
    if (connect(sock, (struct sockaddr *)&name, sizeof(name)))
    {
        SWSS_LOG_ERROR("Ipc client connect error: %s", strerror(errno));
        close(sock);
        return -1;
    }

    struct timeval timeout = {.tv_sec = IPC_TIMEOUT_SEC, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    return sock;
}

IpcServer::IpcServer(const char *sock_path)
    :Ipc(), m_thread(nullptr), m_switch_rid(SAI_NULL_OBJECT_ID), m_switch_api(0)
{
    SWSS_LOG_ENTER();

    m_sock = create(sock_path);
}

IpcServer::~IpcServer()
{
    SWSS_LOG_ENTER();

    endThread();
}

int IpcServer::create(const char *sock_path)
{
    SWSS_LOG_ENTER();

    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        SWSS_LOG_ERROR("socket error: %s", strerror(errno));
        return -1;
    }

    unlink(sock_path);

    struct sockaddr_un name;
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, sock_path, sizeof(name.sun_path) - 1);
    if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
        SWSS_LOG_ERROR("bind error: %s", strerror(errno));
        close(sock);
        return -1;
    }

    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct timeval timeout = {.tv_sec = IPC_TIMEOUT_SEC, .tv_usec = 0};
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    return sock;
}

void IpcServer::startThread()
{
    SWSS_LOG_ENTER();

    if (m_thread == nullptr)
    {
        m_thread = std::make_shared<std::thread>(&IpcServer::threadProc, this);
        SWSS_LOG_INFO("Ipc server thread started");
    }
}

void IpcServer::endThread()
{
    SWSS_LOG_ENTER();

    if (m_thread != nullptr)
    {
        auto thread = std::move(m_thread);
        thread->join();
        SWSS_LOG_INFO("Ipc server thread ended");
    }
}

void IpcServer::threadProc()
{
    SWSS_LOG_ENTER();

    // Sleep a moment for ensuring m_thread OK
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Select select;
    select.addSelectable(this);

    while (m_thread)
    {
        Selectable *sel = nullptr;
        int ret = select.select(&sel, IPC_TIMEOUT_SEC * 1000);

        if (ret == Select::ERROR)
        {
            SWSS_LOG_NOTICE("Ipc server select error %s", strerror(errno));
            continue;
        }

        if (ret == Select::TIMEOUT)
        {
            SWSS_LOG_DEBUG("Ipc server select timeout");
            continue;
        }

        assert(sel == this);
        processMsg();
    }
}

void IpcServer::processMsg()
{
    SWSS_LOG_ENTER();

    union {
        IpcMsg msg;
        char buf[IPC_MSG_MAX_LEN];
    } m;
    IpcMsg *msg = &m.msg;

    struct sockaddr_un peer;
    socklen_t addrlen = sizeof(peer);

    ssize_t num = recvfrom(msg, IPC_MSG_MAX_LEN, (struct sockaddr *)&peer, &addrlen);
    if (num < 0)
    {
        SWSS_LOG_NOTICE("Ipc server recv error %s", strerror(errno));
        return;
    }

    if (num < IPC_MSG_MIN_LEN)
    {
        SWSS_LOG_NOTICE("Ipc server recv %d bytes, too small!", num);
        return;
    }

    auto stub = rpcAt(msg->subtype);
    if (stub)
    {
        stub->process(this, msg, (struct sockaddr *)&peer, addrlen);
    }
    else
    {
        SWSS_LOG_NOTICE("Ipc server: unknown rpc %d", msg->subtype);
    }
}
