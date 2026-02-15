#define SYNCD_IPC_SOCK_SYNCD  "/var/run/sswsyncd"
#define SYNCD_IPC_SOCK_HOST   "/var/run/docker-syncd"
#define SYNCD_IPC_SOCK_FILE   "mdio-ipc"
#define SYNCD_IPC_BUFF_SIZE   256    /* buffer size */

#define MDIO_SERVER_TIMEOUT   30     /* sec, connection timeout */
#define MDIO_CLIENT_TIMEOUT   25     /* shorter than 30 sec on server side */

#define MDIO_CONN_MAX         18     /* max. number of connections */
