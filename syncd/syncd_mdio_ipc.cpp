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
#include <pthread.h>

extern "C" {
#include <sai.h>
#include <saiversion.h>
}

#include "swss/logger.h"

#define SYNCD_IPC_SOCK_SYNCD  "/var/run/sswsyncd"
#define SYNCD_IPC_SOCK_HOST   "/var/run/docker-syncd"
#define SYNCD_IPC_SOCK_FILE   "mdio-ipc"
#define SYNCD_IPC_BUFF_SIZE   256   /* buffer size */

#define CONN_TIMEOUT        30     /* sec, connection timeout */
#define CONN_MAX            18     /* max. number of connections */

#ifndef COUNTOF
#define COUNTOF(x)          ((int)(sizeof((x)) / sizeof((x)[0])))
#endif

extern sai_object_id_t mdioSwitchId;

static pthread_t taskId;
static int taskAlive = 0;

typedef struct syncd_mdio_ipc_conn_s
{
    int    fd;
    time_t timeout;
} syncd_mdio_ipc_conn_t;

typedef struct syncd_mdio_ipc_cmd_s
{
    const char *key;
    int (*func)(char *resp, int argc, char *argv[], sai_switch_api_t *sai_switch_api);
} syncd_mdio_ipc_cmd_t;

/* Remove leading and trailing spaces */
static char *strim(char *s)
{
    size_t len;
    char *end;

    /* Remove the leading spaces */
    while (isspace(*s))
    {
        ++s;
    }

    /* Remove the trailing spaces */
    len = strlen(s);
    if (len == 0)
    {
        return s;
    }
    end = s + len - 1;
    while ((end >= s) && isspace(*end))
    {
        end--;
    }
    *(end + 1) = '\0';

    return s;
}

/* Convert to lowercase string */
static char *strlower(char *s)
{
    unsigned char *c;
    unsigned char *str = (unsigned char *)s;

    for (c = str; *c != 0; ++c)
    {
        *c = (unsigned char)tolower(*c);
    }
    return s;
}

/*
 * mdio <port_oid> reg:     Read from the PHY register
 * mdio <port_oid> reg val: Write to the PHY register
 */
static sai_status_t syncd_ipc_cmd_mdio_common(char *resp, int argc, char *argv[], sai_switch_api_t *sai_switch_api)
{
    int ret = 0;
    uint32_t mdio_addr = 0, reg_addr = 0, val = 0;

    if (argc < 3)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    mdio_addr = (uint32_t)strtoul(argv[1], NULL, 0);
    reg_addr = (uint32_t)strtoul(argv[2], NULL, 0);

    if (mdioSwitchId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("mdioSwitchId not initialized");
        return SAI_STATUS_FAILURE;
    }

    if (argc > 3)
    {
        val = (uint32_t)strtoul(argv[3], NULL, 0);
        ret = sai_switch_api->switch_mdio_write(mdioSwitchId, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d\n", ret);
    }
    else
    {
        ret = sai_switch_api->switch_mdio_read(mdioSwitchId, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d 0x%x\n", ret, val);
    }

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}

#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
static sai_status_t syncd_ipc_cmd_mdio_common_cl22(char *resp, int argc, char *argv[], sai_switch_api_t *sai_switch_api)
{
    int ret = 0;
    uint32_t mdio_addr = 0, reg_addr = 0, val = 0;

    if (argc < 3)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    mdio_addr = (uint32_t)strtoul(argv[1], NULL, 0);
    reg_addr = (uint32_t)strtoul(argv[2], NULL, 0);

    if (mdioSwitchId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("mdioSwitchId not initialized");
        return SAI_STATUS_FAILURE;
    }

    if (argc > 3)
    {
        val = (uint32_t)strtoul(argv[3], NULL, 0);
        ret = sai_switch_api->switch_mdio_write_cl22(mdioSwitchId, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d\n", ret);
    }
    else
    {
        ret = sai_switch_api->switch_mdio_read_cl22(mdioSwitchId, mdio_addr, reg_addr, 1, &val);
        sprintf(resp, "%d 0x%x\n", ret, val);
    }

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}
#endif

static sai_status_t syncd_ipc_cmd_mdio(char *resp, int argc, char *argv[], sai_switch_api_t *sai_switch_api)
{
    return syncd_ipc_cmd_mdio_common(resp, argc, argv, sai_switch_api);
}

static sai_status_t syncd_ipc_cmd_mdio_cl22(char *resp, int argc, char *argv[], sai_switch_api_t *sai_switch_api)
{
#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
    return syncd_ipc_cmd_mdio_common_cl22(resp, argc, argv, sai_switch_api);
#else
    /* In this case, sai configuration should take care of mdio clause 22 */
    return syncd_ipc_cmd_mdio_common(resp, argc, argv, sai_switch_api);
#endif
}

static syncd_mdio_ipc_cmd_t ipc_cmds[] =
{
    { "mdio", syncd_ipc_cmd_mdio },
    { "mdio-cl22", syncd_ipc_cmd_mdio_cl22 },
};

static void *syncd_ipc_task_main(void *ctx)
{
    int i, j, fd, len, ret, sock_srv, sock_cli, sock_max;
    syncd_mdio_ipc_conn_t conn[CONN_MAX];
    struct sockaddr_un addr;
    char path[64];
    fd_set rfds;
    char cmd[SYNCD_IPC_BUFF_SIZE], resp[SYNCD_IPC_BUFF_SIZE], *argv[64], *save;
    int argc = 0;
    sai_switch_api_t *sai_switch_api=(sai_switch_api_t *)ctx;

    strcpy(path, SYNCD_IPC_SOCK_SYNCD);
    fd = open(path, O_DIRECTORY);
    if (fd < 0)
    {
        SWSS_LOG_ERROR("Unable to open the directory %s for IPC\n", path);
        return &errno;
    }

    sock_srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_srv < 0)
    {
        SWSS_LOG_ERROR("socket() returns %d", errno);
        return &errno;
    }

    /***************************************/
    /* Set up the UNIX sockaddr structure  */
    /* by using AF_UNIX for the family and */
    /* giving it a filepath to bind to.    */
    /*                                     */
    /* Unlink the file so the bind will    */
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
        return &errno;
    }

    /* Listen for the upcoming client sockets */
    if (listen(sock_srv, CONN_MAX) < 0)
    {
        SWSS_LOG_ERROR("listen() returns %d", errno);
        unlink(addr.sun_path);
        close(sock_srv);
        return &errno;
    }

    SWSS_LOG_NOTICE("IPC service is online\n");

    memset(conn, 0, sizeof(conn));
    while (taskAlive)
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

        /* reset readfds */
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
            argv[argc++] = strtok_r(strlower(strim(cmd)), " \t\r\n", &save);
            while (argc < COUNTOF(argv))
            {
                argv[argc] = strtok_r(NULL, " \t\r\n", &save);
                if (argv[argc] == NULL)
                    break;
                ++argc;
            }

#if 0
            printf("CMD: ");
            for (j = 0; j < argc; ++j)
                printf("[%s],", argv[j]);
            printf("\n");
#endif

            /* command dispatch */
            resp[0] = 0;
            rc = SAI_STATUS_NOT_SUPPORTED;
            for (j = 0; j < COUNTOF(ipc_cmds); ++j)
            {
                if (strcmp(ipc_cmds[j].key, argv[0]) == 0)
                {
                    rc = (ipc_cmds[j].func)(resp, argc, argv, sai_switch_api);
                    break;
                }
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
    return &errno;
}

void stopIpcMdioProcessingThread(void)
{
    int *err = NULL;

    taskAlive = 0;
    pthread_join(taskId, (void **)&err);
    SWSS_LOG_NOTICE("IPC task thread is stopped\n");
    return;
}

int startIpcMdioProcessingThread(sai_switch_api_t *sai_switch_api)
{
    int err = 0;

    if (!taskAlive)
    {
        taskAlive = 1;
        err = pthread_create(&taskId, NULL, syncd_ipc_task_main, (void*)sai_switch_api);
        if (err != 0)
        {
            taskAlive = 0;
            SWSS_LOG_ERROR("Unable to create IPC task thread");
        }
    }
    return err;
}
