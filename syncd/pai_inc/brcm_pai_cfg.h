/*
 *
 * $Id: brcm_pai_cfg.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef __PAI_CFG_H_
#define __PAI_CFG_H_

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "brcm_pai_common.h"


#define PAI_CFG_STR_MAX        256
#define PAI_CFG_PROP_NAME_MAX  32
#define PAI_CFG_PROP_VALUE_MAX 128

#define PHY_INIT_CFG_CHIP_ID "phy_init_config_chip_id"
#define PHY_INIT_CFG_MODE  "phy_init_config_mode"
#define PHY_INIT_CFG_MACSEC  "phy_init_config_macsec"
#define PHY_INIT_CFG_PLL1  "phy_init_config_pll1"
#define PHY_INIT_CFG_TX_DRV  "phy_init_config_tx_drv"
#define PHY_LANE_PROP "phy_lane_property"
#define PHY_PORT_CFG "phy_port_config"

#define PAI_CFG_PROP_NAMES \
{ \
  PHY_INIT_CFG_CHIP_ID, \
  PHY_INIT_CFG_MODE, \
  PHY_INIT_CFG_MACSEC, \
  PHY_INIT_CFG_PLL1, \
  PHY_INIT_CFG_TX_DRV, \
  PHY_LANE_PROP, \
  PHY_PORT_CFG, \
  "", \
}

#define FREE_CFG_PARAM(_p) if (_p) {                  \
  if ((_p)->param_name) pai_free((_p)->param_name);   \
  if ((_p)->param_value) pai_free((_p)->param_value); \
  pai_free(_p);               \
}

#define PARAM_TYPE_UNDEF       0
#define PARAM_TYPE_GLOBAL      1
#define PARAM_TYPE_PHY         2
#define PARAM_TYPE_LANE        3
#define PARAM_TYPE_PORT        4

typedef struct cfg_param_s {
  char    *param_name;
  char    *param_value;
  int      param_type;
  int      unique_id;
} cfg_param_t;

int pai_cfg_apply(pai_init_t *init);

#endif
