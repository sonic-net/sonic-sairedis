/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "MdioIpcClient.h"

#define SAI_IPC_SOCK_DIR_SYNCD      "/var/run/sswsyncd"
#define SAI_IPC_SOCK_DIR_HOST       "/var/run/docker-syncd"
#define SAI_IPC_SOCK_FILE           "mdio-ipc"
#define SAI_IPC_BUFF_SIZE           256

#define CONN_TIMEOUT                25     /* shorter than 30 sec on server side */

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global variables */

static int syncd_mdio_ipc_command(char *cmd, char *resp)
{
    int fd;
    ssize_t ret;
    size_t len;
    struct sockaddr_un saddr, caddr;
    static int sock = 0;
    static char path[128] = { 0 };
    static time_t timeout = 0;

    if (timeout < time(NULL))
    {
        /* It might already be timed out at the server side, reconnect ... */
        if (sock > 0)
        {
            close(sock);
        }
        sock = 0;
    }

    if (strlen(path) == 0)
    {
        strcpy(path, SAI_IPC_SOCK_DIR_SYNCD);
        fd = open(path, O_DIRECTORY);
        if (fd < 0)
        {
            printf("%s: Program is not run on host\n", __func__);
            strcpy(path, SAI_IPC_SOCK_DIR_HOST);
            fd = open(path, O_DIRECTORY);
            if (fd < 0)
            {
                printf("%s: Unable to open the directory for IPC\n", __func__);
                return errno;
            }
        }
    }

    if (sock <= 0)
    {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("socket()");
            return errno;
        }

        caddr.sun_family = AF_UNIX;
        snprintf(caddr.sun_path, sizeof(caddr.sun_path), "%s/%s.cli.%d", path, SAI_IPC_SOCK_FILE, getpid());
        unlink(caddr.sun_path);
        if (bind(sock, (struct sockaddr *)&caddr, sizeof(caddr)) < 0)
        {
            perror("bind()");
            close(sock);
            sock = 0;
            return errno;
        }

        saddr.sun_family = AF_UNIX;
        snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s/%s.srv", path, SAI_IPC_SOCK_FILE);
        if (connect(sock, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        {
            perror("connect()");
            close(sock);
            sock = 0;
            unlink(caddr.sun_path);
            return errno;
        }
    }

    len = strlen(cmd);
    ret = send(sock, cmd, len, 0);
    if (ret < (ssize_t)len)
    {
        printf("send failed, ret=%ld, expected=%ld\n", ret, len);
        close(sock);
        sock = 0;
        unlink(caddr.sun_path);
        return -EIO;
    }

    ret = recv(sock, resp, SAI_IPC_BUFF_SIZE - 1, 0);
    if (ret <= 0)
    {
        printf("recv failed, ret=%ld\n", ret);
        close(sock);
        sock = 0;
        unlink(caddr.sun_path);
        return -EIO;
    }

    timeout = time(NULL) + CONN_TIMEOUT;
    return (int)strtol(resp, NULL, 0);
}


/* Function to read data from MDIO interface */
extern "C" sai_status_t mdio_read(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, uint32_t *data)
{
    int rc = SAI_STATUS_FAILURE;
    char cmd[SAI_IPC_BUFF_SIZE], resp[SAI_IPC_BUFF_SIZE];

    if (number_of_registers > 1)
    {
        printf("Multiple register reads are not supported, num_of_registers: %d\n", number_of_registers);
        return SAI_STATUS_FAILURE;
    }

    pthread_mutex_lock(&mutex);

    sprintf(cmd, "mdio 0x%x 0x%x\n", mdio_addr, reg_addr);
    if (syncd_mdio_ipc_command(cmd, resp) == 0)
    {
        *data = (uint32_t)strtoul(strchrnul(resp, ' ') + 1, NULL, 0);
        rc = SAI_STATUS_SUCCESS;
    }

    pthread_mutex_unlock(&mutex);
    return rc;
}

/* Function to write data to MDIO interface */
extern "C" sai_status_t mdio_write(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, const uint32_t *data)
{
    int rc = SAI_STATUS_FAILURE;
    char cmd[SAI_IPC_BUFF_SIZE], resp[SAI_IPC_BUFF_SIZE];

    if (number_of_registers > 1)
    {
        printf("Multiple register reads are not supported, num_of_registers: %d\n", number_of_registers);
        return SAI_STATUS_FAILURE;
    }

    pthread_mutex_lock(&mutex);

    sprintf(cmd, "mdio 0x%x 0x%x 0x%x\n", mdio_addr, reg_addr, *data);
    if (syncd_mdio_ipc_command(cmd, resp) == 0)
    {
        rc = SAI_STATUS_SUCCESS;
    }

    pthread_mutex_unlock(&mutex);
    return rc;
}

/* Function to read data using clause 22 from MDIO interface */
extern "C" sai_status_t mdio_read_cl22(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, uint32_t *data)
{
    int rc = SAI_STATUS_FAILURE;
    char cmd[SAI_IPC_BUFF_SIZE], resp[SAI_IPC_BUFF_SIZE];

    if (number_of_registers > 1)
    {
        printf("Multiple register reads are not supported, num_of_registers: %d\n", number_of_registers);
        return SAI_STATUS_FAILURE;
    }

    pthread_mutex_lock(&mutex);

    sprintf(cmd, "mdio-cl22 0x%x 0x%x\n", mdio_addr, reg_addr);
    if (syncd_mdio_ipc_command(cmd, resp) == 0)
    {
        *data = (uint32_t)strtoul(strchrnul(resp, ' ') + 1, NULL, 0);
        rc = SAI_STATUS_SUCCESS;
    }

    pthread_mutex_unlock(&mutex);
    return rc;
}

/* Function to write data using clause 22 to MDIO interface */
extern "C" sai_status_t mdio_write_cl22(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, const uint32_t *data)
{
    int rc = SAI_STATUS_FAILURE;
    char cmd[SAI_IPC_BUFF_SIZE], resp[SAI_IPC_BUFF_SIZE];

    if (number_of_registers > 1)
    {
        printf("Multiple register reads are not supported, num_of_registers: %d\n", number_of_registers);
        return SAI_STATUS_FAILURE;
    }

    pthread_mutex_lock(&mutex);

    sprintf(cmd, "mdio-cl22 0x%x 0x%x 0x%x\n", mdio_addr, reg_addr, *data);
    if (syncd_mdio_ipc_command(cmd, resp) == 0)
    {
        rc = SAI_STATUS_SUCCESS;
    }

    pthread_mutex_unlock(&mutex);
    return rc;
}
