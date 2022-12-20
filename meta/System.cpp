#include "System.h"

#include "swss/logger.h"

#include <stdarg.h>

using namespace saimeta;

int (*System::open_ptr)(const char *pathname, int flags, ...) = nullptr;
int (*System::ioctl_ptr)(int fd, unsigned long request, ...) = nullptr;
int (*System::socket_ptr)(int domain, int type, int protocol) = nullptr;
int (*System::setsockopt_ptr)(int sockfd, int level, int optname, const void *optval, socklen_t optlen) = nullptr;
int (*System::bind_ptr)(int sockfd, const struct sockaddr *addr, socklen_t addrlen) = nullptr;
unsigned int (*System::if_nametoindex_ptr)(const char *ifname);

void System::reset()
{
    SWSS_LOG_ENTER();

    open_ptr = &::open;
    ioctl_ptr = &::ioctl;
    socket_ptr = &::socket;
    setsockopt_ptr = &::setsockopt;
    bind_ptr = &::bind;
}

int System::open(const char *pathname, int flags, ...)
{
    SWSS_LOG_ENTER();

    va_list args;

    va_start(args, flags);

    int ret = (open_ptr == nullptr)
        ? ::open(pathname, flags, args)
        : open_ptr(pathname, flags, args);

    va_end(args);

    return ret;
}

int System::ioctl(int fd, unsigned long request, ...)
{
    SWSS_LOG_ENTER();

    va_list args;

    va_start(args, request);

    int ret = (ioctl_ptr == nullptr)
        ? ::ioctl(fd, request, args)
        : ioctl_ptr(fd, request, args);

    va_end(args);

    return ret;
}

int System::socket(int domain, int type, int protocol)
{
    SWSS_LOG_ENTER();

    return (socket_ptr == nullptr)
        ? ::socket(domain, type, protocol)
        : socket_ptr(domain, type, protocol);
}

int System::setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    SWSS_LOG_ENTER();

    return (setsockopt_ptr == nullptr)
        ? ::setsockopt(sockfd, level, optname, optval, optlen)
        : setsockopt_ptr(sockfd, level, optname, optval, optlen);
}

int System::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    SWSS_LOG_ENTER();

    return (bind_ptr == nullptr)
        ? ::bind(sockfd, addr, addrlen)
        : bind_ptr(sockfd, addr, addrlen);
}

unsigned int System::if_nametoindex(const char *ifname)
{
    SWSS_LOG_ENTER();

    return (if_nametoindex_ptr == nullptr)
        ? ::if_nametoindex(ifname)
        : if_nametoindex_ptr(ifname);
}
