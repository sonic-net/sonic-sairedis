/*********************************************************************
 *
 * Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.
 *
 *********************************************************************/

#if !defined (_BRCM_PAI_EXTENSIONS)
#define _BRCM_PAI_EXTENSIONS

/**
 * @enum sai_port_synce_gen_squelch_config_t
 * @brief Recovered Clock Generation and Squelch Mode Configruation
 */
typedef enum _sai_port_synce_gen_squelch_config_t {
    SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_DISABLE,    /**< Disable Clk Gen and Squelch */
    SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_NONE,       /**< Enable Clk Gen only, no squelch needed (Clock is always sent out) */
    SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_LOS,        /**< Squelch clock on Loss of Signal (LOS) */
    SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_LOL,        /**< Squelch clock on Loss of Lock (LOL) */
    SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_LOS_OR_LOL, /**< Squelch clock on Loss of Signal(LOS) or Loss of Lock(LOL) */
    SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_FORCE       /**< Force Squelch */
} sai_port_synce_gen_squelch_config_t;


/**
 * @enum sai_port_synce_divider_t
 * @brief Divider selection to apply on Line/Lane rate before outputting clock.
 * Not all dividers are supported on each phy.
 * - Dividers supported by Europa family are:
 *    20, 40, 80, 160, 400, 1000
 *
 * - Dividers supported by Millenio family are:
 *    Single-ended ports -> 512,1024, 2048
 *    Differential ports -> 32, 64, 128, 256, 512, 1024, 2048
 *
 * - Dividers supported by Aperta family are:
 *    Single-ended ports -> 80, 120, 240, 520, 1000, 2040, 4080
 *    Differential ports -> 32, 64, 128, 256, 512, 1024, 2048, 4096
 *
 * - Dividers supported by Miura family are:
 *    10G LRM Interface    -> 66, 82.5, 528
 *    All other Interfaces -> 16, 64, 66, 82.5, 528
 */

typedef enum _sai_port_synce_divider_t {
    SAI_PORT_SYNCE_DIVIDER_20,    /**< Divide by 20 */
    SAI_PORT_SYNCE_DIVIDER_40,    /**< Divide by 40*/
    SAI_PORT_SYNCE_DIVIDER_80,    /**< Divide by 80 */
    SAI_PORT_SYNCE_DIVIDER_160,   /**< Divide by 160 */
    SAI_PORT_SYNCE_DIVIDER_400,   /**< Divide by 400 */
    SAI_PORT_SYNCE_DIVIDER_1000,  /**< Divide by 1000 */
    SAI_PORT_SYNCE_DIVIDER_64,    /**< Divide by 64 */
    SAI_PORT_SYNCE_DIVIDER_128,   /**< Divide by 128 */
    SAI_PORT_SYNCE_DIVIDER_256,   /**< Divide by 256 */
    SAI_PORT_SYNCE_DIVIDER_512,   /**< Divide by 512 */
    SAI_PORT_SYNCE_DIVIDER_1024,  /**< Divide by 1024 */
    SAI_PORT_SYNCE_DIVIDER_2048,  /**< Divide by 2048 */
    SAI_PORT_SYNCE_DIVIDER_4096,  /**< Divide by 4096 */
    SAI_PORT_SYNCE_DIVIDER_8192,  /**< Divide by 8192 */
    SAI_PORT_SYNCE_DIVIDER_32,    /**< Divide by 32 */
    SAI_PORT_SYNCE_DIVIDER_120,   /**< Divide by 120 */
    SAI_PORT_SYNCE_DIVIDER_240,   /**< Divide by 240 */
    SAI_PORT_SYNCE_DIVIDER_520,   /**< Divide by 520 */
    SAI_PORT_SYNCE_DIVIDER_2040,  /**< Divide by 2040 */
    SAI_PORT_SYNCE_DIVIDER_4080,  /**< Divide by 4080 */
    SAI_PORT_SYNCE_DIVIDER_1,     /**< Divide by 1. */
    SAI_PORT_SYNCE_DIVIDER_2,     /**< Divide by 2. */
    SAI_PORT_SYNCE_DIVIDER_4,     /**< Divide by 4. */
    SAI_PORT_SYNCE_DIVIDER_8,      /**< Divide by 8. */
    SAI_PORT_SYNCE_DIVIDER_16,     /**< Divide by 16. */
    SAI_PORT_SYNCE_DIVIDER_66,     /**< Divide by 66. */
    SAI_PORT_SYNCE_DIVIDER_82p5,   /**< Divide by 82.5. */
    SAI_PORT_SYNCE_DIVIDER_528    /**< Divide by 528. */
} sai_port_synce_divider_t;

/**
 * @enum brcm_pai_port_mdix_mode_t
 * @brief MDIX Mode
 * This enum has been deprecated by sai_port_mdix_mode_config_t
 * It is kept for backward compatibility purposes.
 */
typedef enum _brcm_pai_port_mdix_mode_t {
    BRCM_PAI_PORT_MDIX_MODE_AUTO = 0,
    BRCM_PAI_PORT_MDIX_MODE_STRAIGHT,
    BRCM_PAI_PORT_MDIX_MODE_CROSSOVER
} brcm_pai_port_mdix_mode_t;

/**
 * @enum brcm_pai_port_auto_neg_mode_t
 * @brief Auto neg mode to configure master or slave mode
 * This enum has been deprecated by sai_port_auto_neg_config_mode_t
 * It is kept for backward compatibility purposes.
 */
typedef enum _brcm_pai_port_auto_neg_mode_t {
    BRCM_PAI_PORT_AUTO_NEG_MODE_DISABLED = 0,
    BRCM_PAI_PORT_AUTO_NEG_MODE_AUTO,
    BRCM_PAI_PORT_AUTO_NEG_MODE_SLAVE,
    BRCM_PAI_PORT_AUTO_NEG_MODE_MASTER
} brcm_pai_port_auto_neg_mode_t;

/**
 * @enum brcm_pai_port_power_mode_t
 * @brief Low or Full Power modes
 */
typedef enum _brcm_pai_port_power_mode_t {
    BRCM_PAI_PORT_POWER_MODE_AUTO = 0,
    BRCM_PAI_PORT_POWER_MODE_LOW,
    BRCM_PAI_PORT_POWER_MODE_FULL
} brcm_pai_port_power_mode_t;

/**
 * @brief Attribute data for #SAI_PORT_ATTR_INTERFACE_TYPE
 * Used for selecting electrical interface with specific electrical pin and signal quality
 */
typedef enum _brcm_pai_port_interface_type_ext_t {

    /** Interface type 1000BaseX */
    BRCM_PAI_PORT_INTERFACE_TYPE_1000X = SAI_PORT_ATTR_CUSTOM_RANGE_START,

    /** Interface type SGMII */
    BRCM_PAI_PORT_INTERFACE_TYPE_SGMII,

    /** Interface type 2500R */
    BRCM_PAI_PORT_INTERFACE_TYPE_2500R,

    /** Interface type 2500X */
    BRCM_PAI_PORT_INTERFACE_TYPE_2500X,

    /** Interface type 5000R */
    BRCM_PAI_PORT_INTERFACE_TYPE_5000R,

    /** Interface type 5000X */
    BRCM_PAI_PORT_INTERFACE_TYPE_5000X,

    /** Interface type USXGMII */
    BRCM_PAI_PORT_INTERFACE_TYPE_USXGMII,

    /** Interface type KRUSXGMII */
    BRCM_PAI_PORT_INTERFACE_TYPE_USXGMII_KR
} brcm_pai_port_interface_type_ext_t;

/* BRCM port specific custom attributes */

/**
 * @brief Low latency variation
 *
 * @type sai_uint32_t
 * @flags CREATE_ONLY 
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_LOW_LATENCY_VARIATION                    (SAI_PORT_ATTR_CUSTOM_RANGE_START)
#define SAI_PORT_ATTR_LOW_LATENCY_VARIATION                         BRCM_PAI_PORT_ATTR_LOW_LATENCY_VARIATION

/**
 * @brief Recovered Clock Generation enable/disable and Squelch Mode Configuration.
 *
 * @type sai_port_synce_gen_squelch_config_t
 * @flags CREATE_AND_SET
 * @default SAI_PORT_SYNCE_GEN_SQUELCH_CONFIG_DISABLE
 */
#define BRCM_PAI_PORT_ATTR_SYNCE_GEN_SQUELCH_CONFIG                 (SAI_PORT_ATTR_CUSTOM_RANGE_START + 2)
#define SAI_PORT_ATTR_SYNCE_GEN_SQUELCH_CONFIG                      BRCM_PAI_PORT_ATTR_SYNCE_GEN_SQUELCH_CONFIG

/**
 * @brief Recovered Clock Lane selection from global lane of phy_id
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_SYNCE_CLOCK_OUTPUT_LANE                  (SAI_PORT_ATTR_CUSTOM_RANGE_START + 3)
#define SAI_PORT_ATTR_SYNCE_CLOCK_OUTPUT_LANE                       BRCM_PAI_PORT_ATTR_SYNCE_CLOCK_OUTPUT_LANE

/**
 * @brief Package Lanes that need to be monitored for Loss of Signal or Loss of Line
 * global lanes of phy_id
 *
 * @type sai_u32_list_t
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_SYNCE_SQUELCH_MONITOR_LANES              (SAI_PORT_ATTR_CUSTOM_RANGE_START + 4)
#define SAI_PORT_ATTR_SYNCE_SQUELCH_MONITOR_LANES                   BRCM_PAI_PORT_ATTR_SYNCE_SQUELCH_MONITOR_LANES

/**
 * @brief Divider selection to apply on Line/Lane rate before outputting clock.
 *
 * @type sai_port_synce_divider_t
 * @flags CREATE_AND_SET
 * @default SAI_PORT_SYNCE_DIVIDER_20
 */
#define BRCM_PAI_PORT_ATTR_SYNCE_DIVIDER                            (SAI_PORT_ATTR_CUSTOM_RANGE_START + 5)
#define SAI_PORT_ATTR_SYNCE_DIVIDER                                 BRCM_PAI_PORT_ATTR_SYNCE_DIVIDER

/**
 * @brief Recovered clock output pins are rclk0 - 0, rclk1 - 1 and rclk2 - 2
 * User must select one of the output pins between 0-2
 *
 * @type sai_uint32_t
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_SYNCE_RCLK_PIN                           (SAI_PORT_ATTR_CUSTOM_RANGE_START + 6)
#define SAI_PORT_ATTR_SYNCE_RCLK_PIN                                BRCM_PAI_PORT_ATTR_SYNCE_RCLK_PIN


/* Copper PHY Attributes */

/**
 * @brief Returns the MDIX status to the user
 * Deprecated by SAI_PORT_ATTR_MDIX_MODE_STATUS.  Kept for backward compatibility purposes.
 *
 * @type brcm_pai_port_mdix_mode_t
 * @flags READ_ONLY
 */
#define BRCM_PAI_PORT_ATTR_MDIX_MODE_STATUS                         (SAI_PORT_ATTR_CUSTOM_RANGE_START + 8)

/**
 * @brief Configure MDIX mode
 * Deprecated by SAI_PORT_ATTR_MDIX_MODE_CONFIG.  Kept for backward compatibility purposes.
 *
 * @type brcm_pai_port_mdix_mode_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_PORT_MDIX_MODE_AUTO
 */
#define BRCM_PAI_PORT_ATTR_MDIX_MODE                                (SAI_PORT_ATTR_CUSTOM_RANGE_START + 9)

/**
 * @brief Configure Auto Neg mode
 * Deprecated by SAI_PORT_ATTR_AUTO_NEG_CONFIG_MODE.  Kept for backward compatibility purposes.
 *
 * @type brcm_pai_port_auto_neg_mode_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_PORT_AUTO_NEG_MODE_DISABLED
 */
#define BRCM_PAI_PORT_ATTR_AUTO_NEG_MODE                            (SAI_PORT_ATTR_CUSTOM_RANGE_START + 10)

/**
 * @brief Configure power mode
 *
 * @type brcm_pai_port_power_mode_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_PORT_POWER_MODE_AUTO
 */
#define BRCM_PAI_PORT_ATTR_POWER_MODE                               (SAI_PORT_ATTR_CUSTOM_RANGE_START + 11)

/**
 * @brief Super isolate mode
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_SUPER_ISOLATE                            (SAI_PORT_ATTR_CUSTOM_RANGE_START + 12)

/**
 * @brief Enable SerDes Auto Negotiation
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_AUTO_NEG_SERDES                          (SAI_PORT_ATTR_CUSTOM_RANGE_START + 14)

/**
 * @brief EEE (AutogrEEEn) Proprietary mode
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_EEE_PROP_MODE                            (SAI_PORT_ATTR_CUSTOM_RANGE_START + 15)

/**
 * @brief EEE latency mode. Fixed latency - False, Variable latency - True
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_EEE_LATENCY_MODE                         (SAI_PORT_ATTR_CUSTOM_RANGE_START + 16)

/**
 * @brief Enable Auto medium to select both Fiber and Copper media types
 * Deprecated by SAI_PORT_ATTR_DUAL_MEDIA.  Kept for backward compatibility purposes.
 *
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_AUTO_MEDIUM                              (SAI_PORT_ATTR_CUSTOM_RANGE_START + 17)

/**
 * @brief Configure media precedence Fiber preferred -> True, Copper preferred -> False
 * Deprecated by SAI_PORT_ATTR_DUAL_MEDIA.  Kept for backward compatibility purposes.
 *
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_MEDIA_PREFERRED                          (SAI_PORT_ATTR_CUSTOM_RANGE_START + 18)

/**
 * @brief Enable 2-pair ethernet mode
 *
 * @type bool
 * @flags CREATE_AND_SET
 * @default false
 */
#define BRCM_PAI_PORT_ATTR_ETH_MODE_2PAIR                           (SAI_PORT_ATTR_CUSTOM_RANGE_START + 19)

/**
 * @brief Attribute data for #BRCM_PAI_PORT_ATTR_CABLE_PAIR_STATE
 * Copper cable pair states
 */
typedef enum _brcm_pai_port_cable_pair_state_t
{
    /**  Cable state no faults */
    BRCM_PAI_PORT_CABLE_PAIR_STATE_OK,

    /**  Cable state open */
    BRCM_PAI_PORT_CABLE_PAIR_STATE_OPEN,

    /**  Cable state short */
    BRCM_PAI_PORT_CABLE_PAIR_STATE_SHORT,

    /**  Cable state cross talk */
    BRCM_PAI_PORT_CABLE_PAIR_STATE_CROSSTALK,

    /**  Cable state unknown */
    BRCM_PAI_PORT_CABLE_PAIR_STATE_UNKNOWN
} brcm_pai_port_cable_pair_state_t;

/**
 * @brief Attribute data for #BRCM_PAI_PORT_ATTR_CABLE_TYPE
 * Copper cable types
 */
typedef enum _brcm_pai_port_cable_type_t
{
    /**  Cable type Unknown */
    BRCM_PAI_PORT_CABLE_TYPE_UNKNOWN,

    /**  Cable type CAT5 */
    BRCM_PAI_PORT_CABLE_TYPE_CAT5,

    /**  Cable type CAT5E */
    BRCM_PAI_PORT_CABLE_TYPE_CAT5E,

    /**  Cable type CAT6 */
    BRCM_PAI_PORT_CABLE_TYPE_CAT6,

    /**  Cable type CAT6A */
    BRCM_PAI_PORT_CABLE_TYPE_CAT6A,

    /**  Cable type CAT7 */
    BRCM_PAI_PORT_CABLE_TYPE_CAT7,
} brcm_pai_port_cable_type_t;

/**
 * @brief Read cable pair state.
 * Returns pair states sequentially from list index 0 to n (n = number of pairs - 1)
 *
 * @type sai_s32_list_t brcm_pai_port_cable_pair_state_t
 * @flags READ_ONLY
 */
#define BRCM_PAI_PORT_ATTR_CABLE_PAIR_STATE                           (SAI_PORT_ATTR_CUSTOM_RANGE_START + 20)

/**
 * @brief Get cable pair length
 * Returns cable pair length sequentially from list index 0 to n (n = number of pairs - 1)
 *
 * @type sai_s32_list_t
 * @flags READ_ONLY
 */
#define BRCM_PAI_PORT_ATTR_CABLE_PAIR_LENGTH                          (SAI_PORT_ATTR_CUSTOM_RANGE_START + 21)

/**
 * @brief Configure cable type to check the cable status
 *
 * @type brcm_pai_port_cable_type_t
 * @flags CREATE_AND_SET
 * @default BRCM_PAI_PORT_CABLE_TYPE_UNKNOWN
 * @validonly SAI_PORT_ATTR_MEDIA_TYPE == SAI_PORT_MEDIA_TYPE_COPPER
 */
#define BRCM_PAI_PORT_ATTR_CABLE_TYPE                                 (SAI_PORT_ATTR_CUSTOM_RANGE_START + 22)

/* Copper PHY attributes end */

/**
 * @brief ChiP reset mode
 *
 * @type bool
 * @flags READ_ONLY
 */
#define BRCM_PAI_SWITCH_ATTR_CHIP_RESET_MODE                        (SAI_SWITCH_ATTR_CUSTOM_RANGE_START)

/**
 * @brief Parallel processing attributes to lock shared resources to achieve thread/process synchronization
 * The attributes data type is void pointer and the format of the function pointers are as follows:
 *
 * int brcm_mutex_lock(uint64_t platform_context)
 *
 * @type sai_pointer_t (void *)brcm_mutex_unlock
 * @flags CREATE_ONLY
 * @default NULL
 */
#define BRCM_PAI_SWITCH_ATTR_SYNC_LOCK                              (SAI_SWITCH_ATTR_CUSTOM_RANGE_START + 1)

/**
 * @brief Parallel processing attributes to unlock shared resources to achieve thread/process synchronization
 * The attributes data type is void pointer and the format of the function pointers are as follows:
 *
 * int brcm_mutex_unlock(uint64_t platform_context)
 *
 * @type sai_pointer_t (void *)brcm_mutex_unlock
 * @flags CREATE_ONLY
 * @default NULL
 */
#define BRCM_PAI_SWITCH_ATTR_SYNC_UNLOCK                            (SAI_SWITCH_ATTR_CUSTOM_RANGE_START + 2)

/**
 * @brief ChiP phy reverse_mode
 *
 * @type bool
 * @flags READ_ONLY
 */
#define BRCM_PAI_SWITCH_ATTR_PHY_REVERSE_MODE                       (SAI_SWITCH_ATTR_CUSTOM_RANGE_START + 3)

#endif /* _BRCM_PAI_EXTENSIONS */
