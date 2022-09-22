#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <fcntl.h>

namespace saimeta
{
    class System
    {
        private:

            System() = delete;
            ~System() = delete;

        public:

            static void reset();

            static int open(const char *pathname, int flags, ...);
            static int ioctl(int fd, unsigned long request, ...);
            static int socket(int domain, int type, int protocol);
            static int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
            static int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
            static unsigned int if_nametoindex(const char *ifname);

        public:

            static int (*open_ptr)(const char *pathname, int flags, ...);
            static int (*ioctl_ptr)(int fd, unsigned long request, ...);
            static int (*socket_ptr)(int domain, int type, int protocol);
            static int (*setsockopt_ptr)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
            static int (*bind_ptr)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
            static unsigned int (*if_nametoindex_ptr)(const char *ifname);
    };
}
