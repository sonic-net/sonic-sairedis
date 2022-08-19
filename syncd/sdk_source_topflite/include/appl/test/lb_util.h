/*
 * $Id: lb_util.h,v 1.1 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * Utilities for Loopback Tests 
 *
 */
#ifndef __LB_UTIL__H
#define __LB_UTIL__H

int lbu_setup_port(int unit, bcm_port_t port, 
                          int req_speed, int autoneg);

#endif /*!__LB_UTIL__H */
