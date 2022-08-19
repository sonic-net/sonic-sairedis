/*
 * $Id: brcm_pai_prop.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 */

#if !defined (__BRCM_PAI_PROP_H_)
#define __BRCM_PAI_PROP_H_


/**
 * @enum brcm_pai_switch_avs_pkg_share_e
 * @brief AVS number of packages share AVS
 */
typedef enum _brcm_pai_switch_avs_pkg_share_e {
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_1, /**< 1:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_2, /**< 2:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_3, /**< 3:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_4, /**< 4:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_5, /**< 5:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_6, /**< 6:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_7, /**< 7:Package share AVS */
    BRCM_PAI_SWITCH_AVS_PKG_SHARE_8, /**< 8:Package share AVS */
} brcm_pai_switch_avs_pkg_share_t;


/**
 * @enum brcm_pai_switch_avs_dc_margin_e
 * @brief AVS Board DC Margin type
 */
typedef enum _brcm_pai_switch_avs_dc_margin_e {
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_0mV, /**< 0 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_5mV, /**< 5 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_10mV, /**< 10 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_15mV, /**< 15 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_20m, /**< 20 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_25mV, /**< 25 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_30mV, /**< 30 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_35mV, /**< 35 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_40mV, /**< 40 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_45mV, /**< 45 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_50mV, /**< 50 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_55mV, /**< 55 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_60mV, /**< 60 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_65mV, /**< 65 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_70mV, /**< 70 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_75mV, /**< 75 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_80mV, /**< 80 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_85mV, /**< 85 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_90mV, /**< 90 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_95mV, /**< 95 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_100mV, /**< 100 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_105mV, /**< 105 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_110mV, /**< 110 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_115mV, /**< 115 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_120mV, /**< 120 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_125mV, /**< 125 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_130mV, /**< 130 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_135mV, /**< 135 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_140mV, /**< 140 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_145mV, /**< 145 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_150mV, /**< 150 mV margin added on top for board IR drop */
    BRCM_PAI_SWITCH_AVS_DC_MARGIN_155mV, /**< 155 mV margin added on top for board IR drop */
} brcm_pai_switch_avs_dc_margin_t;


/**
 * @enum brcm_pai_switch_avs_regulator_e
 * @brief AVS Regulator Type
 */
typedef enum _brcm_pai_switch_avs_regulator_e {
    BRCM_PAI_SWITCH_AVS_REGULATOR_4677,        /**< 4677 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_7219,        /**< 7219 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_7180,        /**< 7180 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_TPS544,      /**< TPS544 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_MAX77812,    /**< MAX77812 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_20730,       /**< 20730 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_4678,        /**< 4678 regulator */
    BRCM_PAI_SWITCH_AVS_REGULATOR_TPS546,      /**< TPS546 regulator */
} brcm_pai_switch_avs_regulator_t;



/**
 * @brief Firmware lane config on a port
 *
 * @type sai_pointer_t (bcm_plp_pm_firmware_lane_config_t *)
 * @flags CREATE_AND_SET
 * @default NULL
 */
#define BRCM_PAI_PORT_ATTR_PROP_FW_LANE_CONFIG                           0xB0000001

/**
 * @brief DSRDS Firmware lane config on a port
 *
 * @type sai_pointer_t bcm_plp_dsrds_firmware_lane_config_t *)
 * @flags CREATE_AND_SET
 * @default NULL
 */
#define BRCM_PAI_PORT_ATTR_PROP_DSRDS_FW_LANE_CONFIG                     0xB0000002

/**
 * @brief Configure the TX FIR
 *
 * @type sai_pointer_t (bcm_plp_tx_t *)
 * @flags CREATE_AND_SET
 * @default NULL
 */
#define BRCM_PAI_PORT_SERDES_ATTR_PROP_TX_FIR                            0xB0000003

/**
 * @brief Configure the RX CONFIG
 *
 * @type sai_pointer_t (bcm_plp_rx_t *)
 * @flags CREATE_AND_SET
 * @default NULL
 */
#define BRCM_PAI_PORT_SERDES_ATTR_PROP_RX_CONFIG                         0xB0000004

/**
 * @brief Enable/Disable AVS on the chip
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_ENABLE                             0xB0000005

/**
 * @brief AVS disable either no firmware control or firmware control
 *  false : No firmware control
 *  true  : Firmware set back voltage
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false: No firmware control
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_DISABLE_TYPE                       0xB0000006

/**
 * @brief AVS Control Type
 *  false : Internal AVS Control
 *  true  : External AVS control
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false: Internal AVS Control
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_CTRL                               0xB0000007

/**
 * @brief AVS number of packages share AVS
 *
 * @type brcm_pai_switch_avs_pkg_share_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_SWITCH_AVS_PKG_SHARE_1
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_PKG_SHARE                          0xB0000008

/**
 * @brief AVS Board DC Margin type
 *
 * @type brcm_pai_switch_avs_dc_margin_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_SWITCH_AVS_DC_MARGIN_0mV
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_DC_MARGIN                          0xB0000009

/**
 * @brief AVS Regulator Type
 *
 * @type brcm_pai_switch_avs_regulator_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_SWITCH_AVS_REGULATOR_4677
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_REGULATOR                          0xB000000A

/**
 * @brief AVS Reference clock in hertz
 *
 * @type sai_uint64_t
 * @flags CREATE_AND_SET
 * @default internal
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_REF_CLK                            0xB000000B

/**
 * @brief AVS I2C slave address in case of external avs regulator as master
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default internal
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_I2C_SLAVE_ADDR_0                   0xB000000C

/**
 * @brief AVS I2C slave address in case of external avs regulator as master
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default internal
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_I2C_SLAVE_ADDR_1                   0xB000000D

/**
 * @brief AVS I2C slave address in case of external avs regulator as master
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default internal
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_I2C_SLAVE_ADDR_2                   0xB000000E


/**
 * @brief AVS External control step 1 or N
 *  false : External control step 1
 *  true  : External control step N
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false: External control step 1
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_EXT_CTRL_STEP                      0xB000000F

/**
 * @brief AVS Regulator I2C slave address
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default internal
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_REGULATOR_I2C_ADDR                 0xB0000010


/**
 * @brief Regulator legs; Enables user to use any of the two legs for supplying DVD
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default internal
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_REGULATOR_LEGS                     0xB0000011

/**
 * @brief master and slave package
 *  false : slave package
 *  true  : master package
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false: slave package
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_INTERNAL_MASTER_SLAVE_PACKAGE      0xB0000013

/**
 * @brief AVS config attributes
 *
 * @type sai_uint32_t
 * @flags READ_ONLY
 */
#define BRCM_PAI_SWITCH_ATTR_PROP_AVS_STATUS                             0xB0000012

#endif
