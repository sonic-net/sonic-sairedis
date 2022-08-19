/*
 * $Id: sysconf.h,v 1.7 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 */

#ifndef __SYSCONF_H__
#define __SYSCONF_H__

#include <sal/types.h>
#include <soc/cmtypes.h>

extern int
sysconf_init(void);

int
sysconf_cm_to_bde_orignal_map_get(int cm_unit, int* bde_unit);

/* Configuration */
extern int
sysconf_probe(void);

extern int
sysconf_attach(int pci_device_n);

extern int
sysconf_detach(int pci_device_n);

extern char*
sysconf_get_property(const char* property);

extern void
sysconf_chip_override(int unit, uint16 *devID, uint8 *revID);
#endif	/*! __SYSCONF_H__ */
