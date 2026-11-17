/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _SONIC_STEL_UAPI_H
#define _SONIC_STEL_UAPI_H

#define SONIC_STEL_FAMILY_NAME "sonic_stel"
#define SONIC_STEL_MCGRP_NAME "ipfix"

enum
{
    SONIC_STEL_CMD_UNSPEC,
    SONIC_STEL_CMD_SEND_IPFIX,
    __SONIC_STEL_CMD_MAX,
};

enum
{
    SONIC_STEL_ATTR_UNSPEC,
    SONIC_STEL_ATTR_IPFIX_DATA,
    __SONIC_STEL_ATTR_MAX,
};

#define SONIC_STEL_ATTR_MAX (__SONIC_STEL_ATTR_MAX - 1)

#endif /* _SONIC_STEL_UAPI_H */
