#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "MdioIpcServer.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <vector>

#define SYNCD_IPC_SOCK_SYNCD  "/var/run/sswsyncd"
#define SYNCD_IPC_SOCK_HOST   "/var/run/docker-syncd"
#define SYNCD_IPC_SOCK_FILE   "mdio-ipc"
#define SYNCD_IPC_BUFF_SIZE   256   /* buffer size */

#define CONN_TIMEOUT        30     /* sec, connection timeout */
#define CONN_MAX            18     /* max. number of connections */

#ifndef COUNTOF
#define COUNTOF(x)          ((int)(sizeof((x)) / sizeof((x)[0])))
#endif

using namespace syncd;

bool MdioIpcServer::m_syncdContext = true;

typedef struct syncd_mdio_ipc_conn_s
{
    int    fd;
    time_t timeout;
} syncd_mdio_ipc_conn_t;

MdioIpcServer::MdioIpcServer(
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ int globalContext):
    m_vendorSai(vendorSai),
    m_switchRid(SAI_NULL_OBJECT_ID),
    m_taskThread(),
    m_taskAlive(0)
{
    SWSS_LOG_ENTER();

    /* globalContext == 0 for syncd, globalContext > 0 for gbsyncd */
    MdioIpcServer::m_syncdContext = (globalContext == 0);
}

MdioIpcServer::~MdioIpcServer()
{
    SWSS_LOG_ENTER();

    m_taskAlive = 0;
    if(m_taskThread.joinable())
    {
        m_taskThread.join();
    }
}

void MdioIpcServer::setSwitchId(
        _In_ sai_object_id_t switchRid)
{
    SWSS_LOG_ENTER();

#ifdef MDIO_ACCESS_USE_NPU
    /* MDIO switch id is only relevant in syncd but not in gbsyncd */
    if (!MdioIpcServer::m_syncdContext)
    {
        return;
    }

    if (m_switchRid != SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("mdio switch id already initialized");
        return;
    }

    m_switchRid = switchRid;

    SWSS_LOG_NOTICE("Initialize mdio switch id with RID = %s",
            sai_serialize_object_id(m_switchRid).c_str());
#endif
}

/*
 * mdio <port_oid> reg:     Read from the PHY register
 * mdio <port_oid> reg val: Write to the PHY register
 */
sai_status_t MdioIpcServer::syncd_ipc_cmd_mdio_common(char *resp, int argc, char *argv[])
{
    int ret = 0;
    uint32_t mdio_addr = 0, reg_addr = 0, val = 0;

    if (argc < 3)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    mdio_addr = (uint32_t)strtoul(argv[1], NULL, 0);
    reg_addr = (uint32_t)strtoul(argv[2], NULL, 0);

    if (m_switchRid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("mdio switch id not initialized");
        return SAI_STATUS_FAILURE;
    }

    if (argc > 3)
    {
        val = (uint32_t)strtoul(argv[3], NULL, 0);
        ret = m_vendorSai->switchMdioWrite(m_switchRid, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d\n", ret);
    }
    else
    {
        ret = m_vendorSai->switchMdioRead(m_switchRid, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d 0x%x\n", ret, val);
    }

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}

#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
sai_status_t MdioIpcServer::syncd_ipc_cmd_mdio_common_cl22(char *resp, int argc, char *argv[])
{
    int ret = 0;
    uint32_t mdio_addr = 0, reg_addr = 0, val = 0;

    if (argc < 3)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    mdio_addr = (uint32_t)strtoul(argv[1], NULL, 0);
    reg_addr = (uint32_t)strtoul(argv[2], NULL, 0);

    if (m_switchRid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("mdio switch id not initialized");
        return SAI_STATUS_FAILURE;
    }

    if (argc > 3)
    {
        val = (uint32_t)strtoul(argv[3], NULL, 0);
        ret = m_vendorSai->switchMdioCl22Write(m_switchRid, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d\n", ret);
    }
    else
    {
        ret = m_vendorSai->switchMdioCl22Read(m_switchRid, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d 0x%x\n", ret, val);
    }

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}
#endif

sai_status_t MdioIpcServer::syncd_ipc_cmd_mdio(char *resp, int argc, char *argv[])
{
    return syncd_ipc_cmd_mdio_common(resp, argc, argv);
}

sai_status_t MdioIpcServer::syncd_ipc_cmd_mdio_cl22(char *resp, int argc, char *argv[])
{
#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
    return MdioIpcServer::syncd_ipc_cmd_mdio_common_cl22(resp, argc, argv);
#else
    /* In this case, sai configuration should take care of mdio clause 22 */
    return MdioIpcServer::syncd_ipc_cmd_mdio_common(resp, argc, argv);
#endif
}

int MdioIpcServer::syncd_ipc_task_main()
{
    SWSS_LOG_ENTER();

    int i;
    int fd;
    int len;
    int ret;
    int sock_srv;
    int sock_cli;
    int sock_max;
    syncd_mdio_ipc_conn_t conn[CONN_MAX];
    struct sockaddr_un addr;
    char path[64];
    fd_set rfds;
    char cmd[SYNCD_IPC_BUFF_SIZE], resp[SYNCD_IPC_BUFF_SIZE], *argv[64], *save;
    int argc = 0;

    strcpy(path, SYNCD_IPC_SOCK_SYNCD);
    fd = open(path, O_DIRECTORY);
    if (fd < 0)
    {
        SWSS_LOG_ERROR("Unable to open the directory %s for IPC\n", path);
        return errno;
    }

    sock_srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_srv < 0)
    {
        SWSS_LOG_ERROR("socket() returns %d", errno);
        return errno;
    }

    /***************************************/
    /* Set up the UNIX socket address      */
    /* by using AF_UNIX for the family and */
    /* giving it a file path to bind to.   */
    /*                                     */
    /* Delete the file so the bind will    */
    /* succeed, then bind to that file.    */
    /***************************************/
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "%s/%s.srv", path, SYNCD_IPC_SOCK_FILE);
    unlink(addr.sun_path);
    if (bind(sock_srv, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        SWSS_LOG_ERROR("bind() returns %d", errno);
        close(sock_srv);
        return errno;
    }

    /* Listen for the upcoming client sockets */
    if (listen(sock_srv, CONN_MAX) < 0)
    {
        SWSS_LOG_ERROR("listen() returns %d", errno);
        unlink(addr.sun_path);
        close(sock_srv);
        return errno;
    }

    SWSS_LOG_NOTICE("IPC service is online\n");

    memset(conn, 0, sizeof(conn));
    while (m_taskAlive)
    {
        time_t now;
        struct timeval timeout;

        /* garbage collection */
        now = time(NULL);
        for (i = 0; i < CONN_MAX; ++i)
        {
            if ((conn[i].fd > 0) && (conn[i].timeout < now))
            {
                SWSS_LOG_NOTICE("socket %d: connection timeout\n", conn[i].fd);
                close(conn[i].fd);
                conn[i].fd = 0;
                conn[i].timeout = 0;
            }
        }

        /* reset read file descriptors */
        FD_ZERO(&rfds);
        FD_SET(sock_srv, &rfds);
        sock_max = sock_srv;
        for (i = 0; i < CONN_MAX; ++i)
        {
            if (conn[i].fd <= 0)
            {
                continue;
            }
            FD_SET(conn[i].fd, &rfds);
            if (sock_max < conn[i].fd)
            {
                sock_max = conn[i].fd;
            }
        }

        /* monitor the socket descriptors */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        ret = select(sock_max + 1, &rfds, NULL, NULL, &timeout);
        if (ret == 0)
        {
            continue;
        }
        else if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                SWSS_LOG_ERROR("select() returns %d", errno);
                break;
            }
        }

        /* Accept the new connection */
        now = time(NULL);
        if (FD_ISSET(sock_srv, &rfds))
        {
            sock_cli = accept(sock_srv, NULL, NULL);
            if (sock_cli <= 0)
            {
                SWSS_LOG_ERROR("accept() returns %d", errno);
                continue;
            }

            for (i = 0; i < CONN_MAX; ++i)
            {
                if (conn[i].fd <= 0)
                {
                    break;
                }
            }
            if (i < CONN_MAX)
            {
                conn[i].fd = sock_cli;
                conn[i].timeout = now + CONN_TIMEOUT;
            }
            else
            {
                SWSS_LOG_ERROR("too many connections!");
                close(sock_cli);
            }
        }

        /* Handle the client requests */
        for (i = 0; i < CONN_MAX; ++i)
        {
            sai_status_t rc = SAI_STATUS_NOT_SUPPORTED;

            sock_cli = conn[i].fd;
            if ((sock_cli <= 0) || !FD_ISSET(sock_cli, &rfds))
            {
                continue;
            }

            /* get the command message */
            len = (int)recv(sock_cli, (void *)cmd, sizeof(cmd) - 1, 0);
            if (len <= 0)
            {
                close(sock_cli);
                conn[i].fd = 0;
                conn[i].timeout = 0;
                continue;
            }
            cmd[len] = 0;

            /* tokenize the command string */
            argc = 0;
            std::string str(cmd);
            boost::algorithm::trim(str);
            boost::algorithm::to_lower(str);
            std::vector<char>v(str.size()+1);
            memcpy( &v.front(), str.c_str(), str.size() + 1 );
            argv[argc++] = strtok_r(v.data(), " \t\r\n", &save);
            while (argc < COUNTOF(argv))
            {
                argv[argc] = strtok_r(NULL, " \t\r\n", &save);
                if (argv[argc] == NULL)
                    break;
                ++argc;
            }

            /* command dispatch */
            resp[0] = 0;
            rc = SAI_STATUS_NOT_SUPPORTED;
            if (argv[0] == NULL)
            {
                rc = SAI_STATUS_NOT_SUPPORTED;
            }
            else if (strcmp("mdio", argv[0]) == 0)
            {
                rc = MdioIpcServer::syncd_ipc_cmd_mdio(resp, argc, argv);
            }
            else if (strcmp("mdio-cl22", argv[0]) == 0)
            {
                rc = MdioIpcServer::syncd_ipc_cmd_mdio_cl22(resp, argc, argv);
            }

            /* build the error message */
            if (rc != SAI_STATUS_SUCCESS)
            {
                sprintf(resp, "%d\n", rc);
            }

            /* send out the response */
            len = (int)strlen(resp);
            if (send(sock_cli, resp, len, 0) < len)
            {
                SWSS_LOG_ERROR("send() returns %d", errno);
            }

            /* update the connection timeout counter */
            conn[i].timeout = time(NULL) + CONN_TIMEOUT;
        }
    }

    /* close socket descriptors */
    for (i = 0; i < CONN_MAX; ++i)
    {
        if (conn[i].fd <= 0)
        {
            continue;
        }
        close(conn[i].fd);
    }
    close(sock_srv);
    unlink(addr.sun_path);
    return errno;
}

void MdioIpcServer::syncd_ipc_task_enter(void *ctx)
{
    SWSS_LOG_ENTER();

    MdioIpcServer *mdioServer = (MdioIpcServer *)ctx;
    mdioServer->syncd_ipc_task_main();
}

void MdioIpcServer::stopMdioThread(void)
{
    SWSS_LOG_ENTER();

#ifdef MDIO_ACCESS_USE_NPU
    /* MDIO IPC server thread is only relevant in syncd but not in gbsyncd */
    if (!MdioIpcServer::m_syncdContext)
    {
        return;
    }

    m_taskAlive = 0;
    m_taskThread.join();
    SWSS_LOG_NOTICE("IPC task thread is stopped\n");
#endif
}

int MdioIpcServer::startMdioThread()
{
    SWSS_LOG_ENTER();

#ifdef MDIO_ACCESS_USE_NPU
    /* MDIO IPC server thread is only relevant in syncd but not in gbsyncd */
    if (!MdioIpcServer::m_syncdContext)
    {
        return 0;
    }

    if (!m_taskAlive)
    {
        m_taskAlive = 1;
        try {
            m_taskThread = std::thread(&MdioIpcServer::syncd_ipc_task_enter, this);
        }
        catch(const std::exception& e) {
            MdioIpcServer::m_taskAlive = 0;
            SWSS_LOG_ERROR("Unable to create IPC task thread: %s", e.what());
            return SAI_STATUS_FAILURE;
        }
    }
#endif
    return SAI_STATUS_SUCCESS;
}
