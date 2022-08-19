/*
 *
 * $Id: brcm_pai_common.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$ 
 *
 *
 */

#if !defined (__BRCM_PAI_COMMON_H_)
#define __BRCM_PAI_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <execinfo.h>
#include <inttypes.h>
#include "sai.h"
#include "epdm.h"
#include "epdm_sec.h"
#include "saimacsec.h"
#include "brcm_pai_log.h"
#include "brcm_pai_port.h"
#include "brcm_pai_extensions.h"

#ifdef BRCM_PAI_PROP
#include "brcm_pai_prop.h"
#endif

#ifdef PAI_SYSLOG_ENABLE
#include <syslog.h>
#endif

/*
 * Externs
 * */
extern sai_log_level_t _adapter_log_level[];
extern sai_service_method_table_t pai_host_services;
extern const sai_switch_api_t pai_switch_apis;
extern const sai_port_api_t pai_port_apis;
extern const sai_macsec_api_t macsec_apis;
extern const sai_acl_api_t macsec_acl_apis;

/*
 * NULL POINTER
 */
#ifndef NULL
#define NULL            (0)
#endif /* NULL */
#ifndef TRUE
#define TRUE      1
#endif
#ifndef FALSE
#define FALSE     0
#endif

#define pai_fgets     fgets
#define pai_fopen     fopen
#define pai_fclose    fclose
#define pai_fread     fread
#define pai_fwrite    fwrite
#define pai_fprintf   fprintf
#define pai_fscanf    fscanf
#define pai_fflush    fflush
#define pai_fseek     fseek
#define pai_ftell     ftell
#define pai_alloc     malloc
#define pai_free      free
#define pai_calloc    calloc
#define pai_memset    memset
#define pai_memcpy    memcpy
#define pai_memcmp    memcmp
#define pai_strncpy   strncpy
#define pai_sprintf   sprintf
#define pai_snprintf  snprintf
#define pai_sscanf    sscanf
#define pai_strncmp   strncmp
#define pai_strcmp    strcmp
#define pai_strlen    strlen
#define pai_strtok    strtok
#define pai_strcpy    strcpy
#define pai_fsync     fsync
#define pai_atoi      atoi
#define pai_puts      puts

#define BRCM_PAI_FILE_RW_OP_ERROR(a, size)                                     \
    do { int bytes_processed;                                                  \
        bytes_processed = a;                                                   \
        if (bytes_processed != size) {                                         \
            PAI_LOG_ERROR(SAI_API_SWITCH, "%s():%d Err: bytes processed:%d\n", \
                __func__, __LINE__, bytes_processed);                          \
            return SAI_STATUS_INSUFFICIENT_RESOURCES;                          \
        }                                                                      \
    } while(0)

#define BCM_PAI_FILE_SEEK_OP_ERROR(a)                                  \
    do { int err;                                                      \
        err = a;                                                       \
        if (err < 0) {                                                 \
            PAI_LOG_ERROR(SAI_API_SWITCH, "%s():%d File seek failed\n",\
                __func__, __LINE__);                                   \
            return SAI_STATUS_INSUFFICIENT_RESOURCES;                  \
        }                                                              \
    } while(0)

#define BRCM_GET_PHY_FROM_SWITCH(S_ID)     (S_ID & 0xFFFFFFFF)
#define BRCM_PAI_SYSTEM_SIDE               1
#define BRCM_PAI_LINE_SIDE                 0

#define BRCM_PAI_API_ID_MIN                (SAI_API_UNSPECIFIED+1)
#define BRCM_PAI_API_ID_MAX                (SAI_API_MAX-1)
#define BRCM_PAI_PORT_SPEED_100G           100000 
#define BRCM_PAI_PORT_SPEED_400G           400000
#define BRCM_PAI_PORT_SPEED_200G           200000 
#define BRCM_PAI_PORT_SPEED_20G            20000 
#define BRCM_PAI_PORT_SPEED_10G            10000 
#define BRCM_PAI_PORT_SPEED_25G            25000 
#define BRCM_PAI_PORT_SPEED_40G            40000
#define BRCM_PAI_PORT_SPEED_50G            50000
#define BRCM_PAI_PORT_SPEED_7P5G           7500
#define BRCM_PAI_PORT_SPEED_5G             5000
#define BRCM_PAI_PORT_SPEED_2P5G           2500
#define BRCM_PAI_PORT_SPEED_1G             1000
#define BRCM_PAI_PORT_SPEED_100M           100
#define BRCM_PAI_PORT_SPEED_10M            10
#define BRCM_PAI_XFIN_MAX_SPEEDS_SUPPORTED 5
#define BRCM_PAI_WTIP_MAX_SPEEDS_SUPPORTED 7

#define BRCM_PAI_PORT_CHIP_INFO_SIZE         256
#define BRCM_PAI_MILN_MILB_TRAINING_ENABLE 0xC00011
#define BRCM_PAI_MILN_CHIP_ID_81724        0x81724
#define BRCM_PAI_MILB_CHIP_ID_81725        0x81725
#define BRCM_PAI_BAR2_CHIP_ID_87728        0x87728
#define BRCM_PAI_BFIN_CHIP_ID_50991        0x50991
#define BRCM_PAI_BFIN_CHIP_ID_50994        0x50994
#define BRCM_PAI_BFIN_CHIP_ID_54991        0x54991
#define BRCM_PAI_BFIN_CHIP_ID_54992        0x54992
#define BRCM_PAI_BFIN_CHIP_ID_54994        0x54994
#define BRCM_PAI_BFIN_CHIP_ID_84891        0x84891
#define BRCM_PAI_BFIN_CHIP_ID_84894        0x84894

#define BRCM_PAI_542XX_MAX_SPEEDS_SUPPORTED 3

#define BRCM_PAI_FEC_EXTENDED_SUPPORT    (0x1 << 8)
#define BRCM_PLP_AN_NEG_FEC_FC         1
#define BRCM_PLP_AN_NEG_RS_FEC         2

#define BRCM_PAI_SINGLE_LANE          1
#define BRCM_PAI_DUAL_LANE            2
#define BRCM_PAI_QUAD_LANE            4

#define BRCM_PAI_LOOPBACK_NONE        0
#define BRCM_PAI_LOOPBACK_DIGITAL     1
#define BRCM_PAI_LOOPBACK_REMOTE      2

#define BRCM_PLP_PORT_CABLE_TYPE_CAT6A      0
#define BRCM_PLP_PORT_CABLE_TYPE_CAT5E      1 

/* Reference Clock in Hz */
#define BRCM_PAI_REF_CLK_106Mhz       106000000
#define BRCM_PAI_REF_CLK_106P25Mhz    106250000
#define BRCM_PAI_REF_CLK_106P5Mhz     106500000
#define BRCM_PAI_REF_CLK_122P88Mhz    122880000
#define BRCM_PAI_REF_CLK_125Mhz       125000000
#define BRCM_PAI_REF_CLK_155Mhz       155000000
#define BRCM_PAI_REF_CLK_155P52Mhz    155520000
#define BRCM_PAI_REF_CLK_156Mhz       156000000
#define BRCM_PAI_REF_CLK_156P6Mhz     156600000
#define BRCM_PAI_REF_CLK_156P637Mhz   156637000
#define BRCM_PAI_REF_CLK_157Mhz       157000000
#define BRCM_PAI_REF_CLK_157P844Mhz   157844000
#define BRCM_PAI_REF_CLK_158Mhz       158000000
#define BRCM_PAI_REF_CLK_158P51Mhz    158510000
#define BRCM_PAI_REF_CLK_159Mhz       159000000
#define BRCM_PAI_REF_CLK_159P375Mhz   159375000
#define BRCM_PAI_REF_CLK_161Mhz       161000000
#define BRCM_PAI_REF_CLK_162P26Mhz    162260000
#define BRCM_PAI_REF_CLK_162P948Mhz   162948000
#define BRCM_PAI_REF_CLK_167P331Mhz   167331000
#define BRCM_PAI_REF_CLK_167P38Mhz    167380000
#define BRCM_PAI_REF_CLK_167P41Mhz    167410000
#define BRCM_PAI_REF_CLK_168Mhz       168000000
#define BRCM_PAI_REF_CLK_168P04Mhz    168040000
#define BRCM_PAI_REF_CLK_168P12Mhz    168120000
#define BRCM_PAI_REF_CLK_169P409Mhz   169409000
#define BRCM_PAI_REF_CLK_172Mhz       172000000
#define BRCM_PAI_REF_CLK_173Mhz       173000000
#define BRCM_PAI_REF_CLK_173P37Mhz    173370000
#define BRCM_PAI_REF_CLK_174Mhz       174000000
#define BRCM_PAI_REF_CLK_174P70Mhz    174700000
#define BRCM_PAI_REF_CLK_212P5Mhz     212500000
#define BRCM_PAI_REF_CLK_311P04Mhz    311040000
#define BRCM_PAI_REF_CLK_312Mhz       312000000
#define BRCM_PAI_REF_CLK_322Mhz       322000000
#define BRCM_PAI_REF_CLK_334P66Mhz    334660000
#define BRCM_PAI_REF_CLK_336P094Mhz   336094000
#define BRCM_PAI_REF_CLK_345P28Mhz    345280000
#define BRCM_PAI_REF_CLK_346P74Mhz    346740000
#define BRCM_PAI_REF_CLK_348P125Mhz   348125000
#define BRCM_PAI_REF_CLK_349Mhz       349000000
#define BRCM_PAI_REF_CLK_425Mhz       425000000
#define BRCM_PAI_REF_CLK_644Mhz       644000000
#define BRCM_PAI_REF_CLK_698Mhz       698000000

/* default latency value in CU PHYs */
#define EEE_LATENCY_VALUE_DEF         0x047E
#define EEE_IDLE_THRESHOLD_DEF        0x3D09
/* SAI header value is in microseconds */
/* Multiplying by 1000 to convert it to nanoseconds */
/* EEE resolution is 6.4 nanoseconds so multiplying with 10 to avoid floating point */
#define EEE_SAI_TO_PHY_RESOLUTION(x)  ((x*1000*10)/64)
#define EEE_PHY_TO_SAI_RESOLUTION(x)  ((x*64)/10000)

#define PAI_VER "3.5"

/* Error check macros */
#define IS_NULL(ptr) (NULL == ptr)
#define COUNTOF(ary)        ((int) (sizeof (ary) / sizeof ((ary)[0])))

typedef enum boot_flags_e {
    BOOT_FLAGS_COLD = 0,
    BOOT_FLAGS_WARM,
    BOOT_FLAGS_FASTFAST,
} boot_flags_t;

typedef struct pai_init_s
{
    boot_flags_t boot_flags;  /* SAI boot up flags*/
    char         *cfg_fname; /* Absolute path and config file name */
    char         *wb_fname;  /* Absolute path and warmboot file name */
    int          phy_id;
} pai_init_t;

typedef enum PAI_API_OPERATION_E
{
    PAI_API_OP_SET = 0,
    PAI_API_OP_GET,
    PAI_API_OP_MAX
} PAI_API_OPERATION_T;

typedef enum PAI_FEC_COUNTER_TYPE_E
{
    FEC_COUNTER_CORRECTABLE = 0,
    FEC_COUNTER_UNCORRECTABLE
} PAI_FEC_COUNTER_TYPE_T;

/*                      Object id definition
 * +--------------|--------------|--------------|-----------------------+
 * |63..........56|55..........36|35..........32|31....................0|
 * +--------------|--------------|--------------|-----------------------+
 * |SAI Id (8bits)| Type(20bits) |Subtype(4bits)|       Id (32 bits)    |
 * +--------------|---------|---------|---------------------------------+
 */
/*
 * Switch object Id format
 * Obj Id = Switch Object Id
 * Type = BRCM OUI
 * Subtype = NA
 * Id = PHY ID
 * */

/*
 * Port object Id format
 * Obj Id = Port Object Id
 * Type = 20 Bit Chip Id
 * Subtype = 4 bit if_Side (Line/System)
 * Id = Port Number
 * */

/*
 * Port Connector Id format
 * Obj Id = Port Connector Object Id
 * Type = NA
 * Subtype = NA
 * Id = Line Side Port Number
 * */

/* MACsec Object Id format
 * Obj Id = MACsec Object Id
 * Type = NA
 * Subtype = Direction (Ingress/Egress)
 * Id = Line Side Port Number
 * */

/*
 * Lane Object Id format (This object is only created for internal consumption
 *                        and there is no corresponding SAI object available.
 *                        This will be mainly used for saving lane configurations)
 * Obj Id = Internal Lane Object Id
 * Type = NA
 * Subtype = if_side (system or line side)
 * Id = Global lane number
 * */
#define BRCM_PAI_CREATE_OBJ_ID(obj_id, type, subtype, id) ((((sai_object_id_t)obj_id) << 56) | \
                                                                (((sai_object_id_t)type) << 36) | \
                                                                (((sai_object_id_t)subtype) << 32) | id)

#define BRCM_PAI_GET_OBJ_SAI_ID(object_id)   ((uint8_t)(object_id >> 56))
#define BRCM_PAI_GET_OBJ_TYPE(object_id)     ((uint32_t)(object_id >> 36) & 0xFFFFF)
#define BRCM_PAI_GET_OBJ_SUB_TYPE(object_id) ((uint8_t)(object_id >> 32) & 0xF)
#define BRCM_PAI_GET_OBJ_ID(object_id)       ((uint32_t)(object_id))

/* Supported speed enum */
#define SPD_10M   (1 << 0)
#define SPD_100M  (1 << 1)
#define SPD_1G    (1 << 2)
#define SPD_2P5G  (1 << 3)
#define SPD_5G    (1 << 4)
#define SPD_7P5G  (1 << 5)
#define SPD_10G   (1 << 6)
#define SPD_25G   (1 << 7)
#define SPD_40G   (1 << 8)
#define SPD_50G   (1 << 9)
#define SPD_100G  (1 << 10)
#define SPD_200G  (1 << 11)
#define SPD_400G  (1 << 12)

typedef struct brcm_pai_phy_part_to_name_t {
    char part_number[BRCM_MAX_CHAR_CHIP_NAME]; /* may be similar to chip_id. Added for ease of use in config*/
    int chip_id;
    char chip_name[BRCM_MAX_CHAR_CHIP_NAME];
    uint8_t sys_side_pkg_lanes;
    uint8_t line_side_pkg_lanes;
} brcm_pai_phy_part_to_name_t;

#define BRCM_PAI_IF_ERR_RETURN(a)                                      \
    do { int err;                                                      \
        err = a;                                                       \
        if (err != SAI_STATUS_SUCCESS) {                               \
            PAI_LOG_ERROR(SAI_API_SWITCH, "%s():%d failed, error:%d\n",\
                __func__, __LINE__, err);                              \
            return err;                                                \
        }                                                              \
    } while(0)

#define BRCM_PAI_EPDM_IF_ERR_RETURN(a)                                 \
    do { int err;                                                      \
        err = a;                                                       \
        if (err != SAI_STATUS_SUCCESS){                                \
            PAI_LOG_ERROR(SAI_API_SWITCH, "%s():%d failed, error:%d\n",\
                __func__, __LINE__, err);                              \
            return brcm_epdm_err_to_pai(err);}                         \
    } while(0)


#define BRCM_PAI_IS_SYSTEM_SIDE(IF_SIDE)    (IF_SIDE == BRCM_PAI_SYSTEM_SIDE)
#define BRCM_PAI_IS_LINE_SIDE(IF_SIDE)      (IF_SIDE == BRCM_PAI_LINE_SIDE)

#define BRCM_PAI_APERTA                       "aperta"
#define BRCM_PAI_MILN                         "millenio"
#define BRCM_PAI_MILB                         "milleniob"
#define BRCM_PAI_EVORA                        "evora"
#define BRCM_PAI_EUROPA                       "europa"
#define BRCM_PAI_MIURA                        "miura"
#define BRCM_PAI_ESTQ                         "estoque"
#define BRCM_PAI_ESTQJ                        "estoquej"
#define BRCM_PAI_BARC                         "barchetta"
#define BRCM_PAI_Q28                          "quadra28"
#define BRCM_PAI_BAR2                         "barchetta2"
#define BRCM_PAI_BRNC                         "broncos"
#define BRCM_PAI_GNTS                         "giants"
#define BRCM_PAI_CBYS                         "cowboys"
#define BRCM_PAI_RAMS                         "rams"
#define BRCM_PAI_RAVN                         "ravens"
#define BRCM_PAI_SHWK                         "seahawks"
#define BRCM_PAI_BFIN                         "blackfin"
#define BRCM_PAI_SFIN                         "shortfin"
#define BRCM_PAI_LFIN                         "longfin"
#define BRCM_PAI_BRFN                         "broadfin"
#define BRCM_PAI_WTIP                         "whitetip"

#define BRCM_PAI_Q28_SUPPORT(chip_name)        (pai_strncmp(chip_name, BRCM_PAI_Q28, pai_strlen(BRCM_PAI_Q28)) == 0)
#define BRCM_PAI_ESTQ_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_ESTQ, pai_strlen(BRCM_PAI_ESTQ)) == 0)
#define BRCM_PAI_BARC_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_BARC, pai_strlen(BRCM_PAI_BARC)) == 0)
#define BRCM_PAI_ESTQJ_SUPPORT(chip_name)      (pai_strncmp(chip_name, BRCM_PAI_ESTQJ, pai_strlen(BRCM_PAI_ESTQJ)) == 0)
#define BRCM_PAI_BAR2_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_BAR2, pai_strlen(BRCM_PAI_BAR2)) == 0)
#define BRCM_PAI_EVORA_SUPPORT(chip_name)      (pai_strncmp(chip_name, BRCM_PAI_EVORA, pai_strlen(BRCM_PAI_EVORA)) == 0)
#define BRCM_PAI_MIURA_SUPPORT(chip_name)      (pai_strncmp(chip_name, BRCM_PAI_MIURA, pai_strlen(BRCM_PAI_MIURA)) == 0)
#define BRCM_PAI_EUROPA_SUPPORT(chip_name)     (pai_strncmp(chip_name, BRCM_PAI_EUROPA, pai_strlen(BRCM_PAI_EUROPA)) == 0)
#define BRCM_PAI_APERTA_SUPPORT(chip_name)     (pai_strncmp(chip_name, BRCM_PAI_APERTA, pai_strlen(BRCM_PAI_APERTA)) == 0)
#define BRCM_PAI_MILN_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_MILN, pai_strlen(BRCM_PAI_MILN)) == 0)
#define BRCM_PAI_MILB_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_MILB, pai_strlen(BRCM_PAI_MILB)) == 0)
#define BRCM_PAI_RAVN_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_RAVN, pai_strlen(BRCM_PAI_RAVN)) == 0)
#define BRCM_PAI_BFIN_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_BFIN, pai_strlen(BRCM_PAI_BFIN)) == 0)
#define BRCM_PAI_SFIN_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_SFIN, pai_strlen(BRCM_PAI_SFIN)) == 0)
#define BRCM_PAI_LFIN_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_LFIN, pai_strlen(BRCM_PAI_LFIN)) == 0)
#define BRCM_PAI_BRFN_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_BRFN, pai_strlen(BRCM_PAI_BRFN)) == 0)
#define BRCM_PAI_WTIP_SUPPORT(chip_name)       (pai_strncmp(chip_name, BRCM_PAI_WTIP, pai_strlen(BRCM_PAI_WTIP)) == 0)

#define PAI_STRNCMP(chip_name, str)            (pai_strncmp(chip_name, str, pai_strlen(str)) == 0)

#define CABLE_PAIR_MAX                        4

/* 10G CU PHYs use bits 15:31 for master port address.
 * Master Port Address
 * bit 31:26 - reserved
 * bit 25:21 - MDIO bus ID
 * bit 20:16 - MDIO addr
 * Current Port Address
 * bit 15:10 - reserved
 * bit 9:5   - MDIO bus ID
 * bit 4:0   - MDIO addr
 *
 * HSIP PHYs do not use bits 15:31 */
#define MASTER_PORT_ID_SHIFT              16
#define BRCM_PAI_GET_PHY_ADDR(PHY_ADDR)   (PHY_ADDR & 0xFFFF)
#define BRCM_PAI_GET_LANE_MASK(MAX_LN)    ((1 << MAX_LN) - 1)

#define BRCM_PAI_ID_CHIP_NAME_INIT_VAR    {{"81388",   0x81388,BRCM_PAI_APERTA, 8, 8}, \
                                           {"81343",   0x81343,BRCM_PAI_APERTA, 8, 8}, \
                                           {"81384",   0x81384,BRCM_PAI_APERTA, 8, 8}, \
                                           {"81385",   0x81385,BRCM_PAI_APERTA, 8, 8}, \
                                           {"81394",   0x81394,BRCM_PAI_APERTA, 8, 8}, \
                                           {"81724",   0x81724,BRCM_PAI_MILN, 8, 16}, \
                                           {"81725",   0x81725,BRCM_PAI_MILB, 8, 16}, \
                                           {"81356",   0x81356,BRCM_PAI_MILB, 16, 16}, \
                                           {"81358",   0x81358,BRCM_PAI_MILB, 8, 8}, \
                                           {"82391",   0x82391,BRCM_PAI_EVORA, 4, 4}, \
                                           {"82392",   0x82392,BRCM_PAI_EVORA, 4, 4}, \
                                           {"82394",   0x82394,BRCM_PAI_EVORA, 4, 4}, \
                                           {"59202",   0x59202,BRCM_PAI_EVORA, 4, 4}, \
                                           {"82396",   0x82396,BRCM_PAI_EUROPA, 4, 4},\
                                           {"82397",   0x82397,BRCM_PAI_EUROPA, 4, 4},\
                                           {"82398",   0x82398,BRCM_PAI_EUROPA, 4, 4},\
                                           {"82399",   0x82399,BRCM_PAI_EUROPA, 4, 4},\
                                           {"82755",   0x82755,BRCM_PAI_MIURA, 4, 4}, \
                                           {"82756",   0x82756,BRCM_PAI_MIURA, 4, 4}, \
                                           {"82757",   0x82757,BRCM_PAI_MIURA, 2, 2}, \
                                           {"82759",   0x82759,BRCM_PAI_MIURA, 2, 2}, \
                                           {"81328",   0x81328,BRCM_PAI_ESTQ, 8, 8}, \
                                           {"81329",   0x81329,BRCM_PAI_ESTQ, 8, 8}, \
                                           {"81330",   0x81330,BRCM_PAI_ESTQ, 8, 8}, \
                                           {"81188",   0x81188,BRCM_PAI_ESTQ, 8, 8}, \
                                           {"59850",   0x59850,BRCM_PAI_ESTQJ, 8, 8},\
                                           {"81321",   0x81321,BRCM_PAI_BARC, 8, 8}, \
                                           {"81337",   0x81337,BRCM_PAI_BARC, 8, 8}, \
                                           {"81338",   0x81338,BRCM_PAI_BARC, 8, 8}, \
                                           {"81381",   0x81381,BRCM_PAI_BARC, 8, 8}, \
                                           {"81764",   0x81764,BRCM_PAI_BARC, 8, 8}, \
                                           {"82780",   0x82780,BRCM_PAI_Q28, 4, 4}, \
                                           {"82758",   0x82758,BRCM_PAI_Q28, 4, 4}, \
                                           {"87326",   0x87326,BRCM_PAI_BAR2, 16, 16},\
                                           {"87328",   0x87328,BRCM_PAI_BAR2, 8, 8},\
                                           {"87728",   0x87728,BRCM_PAI_BAR2, 8, 16},\
                                           {"50991EL", 0x50991,BRCM_PAI_BFIN, 1, 1},\
                                           {"54991E",  0x54991,BRCM_PAI_BFIN, 1, 1},\
                                           {"54991EL", 0x54991,BRCM_PAI_BFIN, 1, 1},\
                                           {"50991L",  0x50991,BRCM_PAI_BFIN, 1, 1},\
                                           {"54991L",  0x54991,BRCM_PAI_BFIN, 1, 1},\
                                           {"54991",   0x54991,BRCM_PAI_BFIN, 1, 1},\
                                           {"54991HA", 0x54991,BRCM_PAI_BFIN, 1, 1},\
                                           {"84891",   0x84891,BRCM_PAI_BFIN, 1, 1},\
                                           {"84891L",  0x84891,BRCM_PAI_BFIN, 1, 1},\
                                           {"54992",   0x54992,BRCM_PAI_BFIN, 2, 2},\
                                           {"54992E",  0x54992,BRCM_PAI_BFIN, 2, 2},\
                                           {"84892",   0x84892,BRCM_PAI_BFIN, 2, 2},\
                                           {"50994E",  0x50994,BRCM_PAI_BFIN, 4, 4},\
                                           {"54994E",  0x54994,BRCM_PAI_BFIN, 4, 4},\
                                           {"54994",   0x54994,BRCM_PAI_BFIN, 4, 4},\
                                           {"54994HA", 0x54994,BRCM_PAI_BFIN, 4, 4},\
                                           {"84894",   0x84894,BRCM_PAI_BFIN, 4, 4},\
                                           {"54994EL", 0x54994,BRCM_PAI_SFIN, 4, 4},\
                                           {"50998E",  0x50998,BRCM_PAI_SFIN, 8, 8},\
                                           {"50998ES", 0x50998,BRCM_PAI_SFIN, 8, 8},\
                                           {"54998E",  0x54998,BRCM_PAI_SFIN, 8, 8},\
                                           {"54998ES", 0x54998,BRCM_PAI_SFIN, 8, 8},\
                                           {"50998S",  0x50998,BRCM_PAI_SFIN, 8, 8},\
                                           {"54998",   0x54998,BRCM_PAI_SFIN, 8, 8},\
                                           {"54998S",  0x54998,BRCM_PAI_SFIN, 8, 8},\
                                           {"84896",   0x84896,BRCM_PAI_SFIN, 8, 8},\
                                           {"84898",   0x84898,BRCM_PAI_SFIN, 8, 8},\
                                           {"50991ELM",0x50991,BRCM_PAI_LFIN, 1, 1},\
                                           {"54991SK", 0x54991,BRCM_PAI_LFIN, 1, 1},\
                                           {"54991ELM",0x54991,BRCM_PAI_LFIN, 1, 1},\
                                           {"54991EM", 0x54991,BRCM_PAI_LFIN, 1, 1},\
                                           {"54991LM", 0x54991,BRCM_PAI_LFIN, 1, 1},\
                                           {"54991M",  0x54991,BRCM_PAI_LFIN, 1, 1},\
                                           {"54991HB", 0x54991,BRCM_PAI_LFIN, 1, 1},\
                                           {"84891LM", 0x84891,BRCM_PAI_LFIN, 1, 1},\
                                           {"84891M",  0x84891,BRCM_PAI_LFIN, 1, 1},\
                                           {"54992EM", 0x54992,BRCM_PAI_LFIN, 2, 2},\
                                           {"54992M",  0x54992,BRCM_PAI_LFIN, 2, 2},\
                                           {"84892M",  0x84892,BRCM_PAI_LFIN, 2, 2},\
                                           {"54994EM", 0x54994,BRCM_PAI_LFIN, 4, 4},\
                                           {"54994M",  0x54994,BRCM_PAI_LFIN, 4, 4},\
                                           {"84894M",  0x84894,BRCM_PAI_LFIN, 4, 4},\
                                           {"54994HB", 0x54991,BRCM_PAI_LFIN, 4, 4},\
                                           {"54994SK", 0x54994,BRCM_PAI_LFIN, 4, 4},\
                                           {"84901",   0x84901,BRCM_PAI_WTIP, 1, 1},\
                                           {"84901L",  0x84901,BRCM_PAI_WTIP, 1, 1},\
                                           {"84902",   0x84902,BRCM_PAI_WTIP, 2, 2},\
                                           {"84904",   0x84904,BRCM_PAI_WTIP, 4, 4},\
                                           {"84891P",  0x84891,BRCM_PAI_WTIP, 1, 1},\
                                           {"84891LP", 0x84891,BRCM_PAI_WTIP, 1, 1},\
                                           {"84892P",  0x84892,BRCM_PAI_WTIP, 2, 2},\
                                           {"84894P",  0x84894,BRCM_PAI_WTIP, 4, 4},\
                                           {"54991P",  0x54991,BRCM_PAI_WTIP, 1, 1},\
                                           {"54991EP", 0x54991,BRCM_PAI_WTIP, 1, 1},\
                                           {"54991LP", 0x54991,BRCM_PAI_WTIP, 1, 1},\
                                           {"54991ELP",0x54991,BRCM_PAI_WTIP, 1, 1},\
                                           {"54991HP", 0x54991,BRCM_PAI_WTIP, 1, 1},\
                                           {"54992P",  0x54992,BRCM_PAI_WTIP, 2, 2},\
                                           {"54992EP", 0x54992,BRCM_PAI_WTIP, 2, 2},\
                                           {"54994P",  0x54994,BRCM_PAI_WTIP, 4, 4},\
                                           {"54994EP", 0x54994,BRCM_PAI_WTIP, 4, 4},\
                                           {"54994HP", 0x54994,BRCM_PAI_WTIP, 4, 4},\
                                           {"54998ESM",0x54998,BRCM_PAI_BRFN, 8, 8},\
                                           {"54998EM", 0x54998,BRCM_PAI_BRFN, 8, 8},\
                                           {"54998SM", 0x54998,BRCM_PAI_BRFN, 8, 8},\
                                           {"54998M",  0x54998,BRCM_PAI_BRFN, 8, 8},\
                                           {"84898M",  0x84898,BRCM_PAI_BRFN, 8, 8},\
                                           {"50140",   0x50140,BRCM_PAI_BRNC, 4, 4},\
                                           {"54140",   0x54140,BRCM_PAI_BRNC, 4, 4},\
                                           {"50180",   0x50180,BRCM_PAI_BRNC, 8, 8},\
                                           {"54180",   0x54180,BRCM_PAI_BRNC, 8, 8},\
                                           {"50182",   0x50182,BRCM_PAI_BRNC, 8, 8},\
                                           {"54182",   0x54182,BRCM_PAI_BRNC, 8, 8},\
                                           {"50185",   0x50185,BRCM_PAI_BRNC, 8, 8},\
                                           {"54185",   0x54185,BRCM_PAI_BRNC, 8, 8},\
                                           {"54280",   0x54280,BRCM_PAI_GNTS, 8, 8},\
                                           {"50280",   0x50280,BRCM_PAI_GNTS, 8, 8},\
                                           {"54285",   0x54285,BRCM_PAI_GNTS, 8, 8},\
                                           {"50285",   0x50285,BRCM_PAI_GNTS, 8, 8},\
                                           {"54282",   0x54282,BRCM_PAI_GNTS, 8, 8},\
                                           {"50282",   0x50282,BRCM_PAI_GNTS, 8, 8},\
                                           {"54240",   0x54240,BRCM_PAI_GNTS, 4, 4},\
                                           {"50240",   0x50240,BRCM_PAI_GNTS, 4, 4},\
                                           {"54210",   0x54210,BRCM_PAI_RAMS, 1, 1},\
                                           {"54210S",  0x54210,BRCM_PAI_RAMS, 1, 1},\
                                           {"54210SE", 0x54210,BRCM_PAI_RAMS, 1, 1},\
                                           {"50210",   0x50210,BRCM_PAI_RAMS, 1, 1},\
                                           {"50210F",  0x50210,BRCM_PAI_RAMS, 1, 1},\
                                           {"50210S",  0x50210,BRCM_PAI_RAMS, 1, 1},\
                                           {"54219S",  0x54219,BRCM_PAI_RAMS, 1, 1},\
                                           {"54219SE", 0x54219,BRCM_PAI_RAMS, 1, 1},\
                                           {"54220",   0x54220,BRCM_PAI_RAMS, 2, 2},\
                                           {"50220",   0x50220,BRCM_PAI_RAMS, 2, 2},\
                                           {"54220S",  0x54220,BRCM_PAI_RAMS, 2, 2},\
                                           {"50220S",  0x50220,BRCM_PAI_RAMS, 2, 2},\
                                           {"54220SE", 0x54220,BRCM_PAI_RAMS, 2, 2},\
                                           {"54290",   0x54290,BRCM_PAI_RAVN, 8, 8},\
                                           {"50290",   0x50290,BRCM_PAI_RAVN, 8, 8},\
                                           {"54291",   0x54291,BRCM_PAI_RAVN, 4, 4},\
                                           {"50291",   0x50291,BRCM_PAI_RAVN, 4, 4},\
                                           {"54292",   0x54292,BRCM_PAI_RAVN, 8, 8},\
                                           {"50292",   0x50292,BRCM_PAI_RAVN, 8, 8},\
                                           {"54293",   0x54293,BRCM_PAI_RAVN, 4, 4},\
                                           {"54294",   0x54294,BRCM_PAI_RAVN, 4, 4},\
                                           {"50294",   0x50294,BRCM_PAI_RAVN, 4, 4},\
                                           {"54295",   0x54295,BRCM_PAI_RAVN, 8, 8},\
                                           {"50295",   0x50295,BRCM_PAI_RAVN, 8, 8},\
                                           {"54296",   0x54296,BRCM_PAI_RAVN, 4, 4},\
                                           {"50296",   0x50296,BRCM_PAI_RAVN, 4, 4},\
                                           {"54190",   0x54190,BRCM_PAI_SHWK, 8, 8},\
                                           {"54192",   0x54192,BRCM_PAI_SHWK, 8, 8},\
                                           {"54194",   0x54194,BRCM_PAI_SHWK, 4, 4},\
                                           {"54195",   0x54195,BRCM_PAI_SHWK, 8, 8},\
                                           {"54210E",  0x54210,BRCM_PAI_CBYS, 1, 1},\
                                           {"54210PE", 0x54210,BRCM_PAI_CBYS, 1, 1},\
                                           {"50210E",  0x50210,BRCM_PAI_CBYS, 1, 1},\
                                           {"50212E",  0x50212,BRCM_PAI_CBYS, 1, 1},\
                                           {"54213PE", 0x54213,BRCM_PAI_CBYS, 1, 1},\
                                           {"50213PE", 0x54213,BRCM_PAI_CBYS, 1, 1},\
                                           {"54214E",  0x54214,BRCM_PAI_CBYS, 1, 1},\
                                           {"54216E",  0x54216,BRCM_PAI_CBYS, 1, 1},\
                                           {"54216EC", 0x54216,BRCM_PAI_CBYS, 1, 1}}



#define BRCM_PAI_COUNT_SET_BITS(DATA1,NO)                         \
    {                                                             \
        int DATA = DATA1;                                         \
        DATA = DATA - ((DATA >> 1) & 0X55555555);                 \
        DATA = (DATA & 0X33333333) + ((DATA >> 2) & 0X33333333);  \
        DATA = ((DATA + (DATA>> 4)) & 0X0F0F0F0F);                \
        NO = (DATA * 0x01010101) >> 24;                           \
    }

#define BRCM_PAI_IF_TYPE_EPDM_IF(SAI_IF,EPDM_IF)                        \
        if (SAI_IF == SAI_PORT_INTERFACE_TYPE_CR ) {                    \
            EPDM_IF = bcm_pm_InterfaceCR;                               \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_CR4) {             \
            EPDM_IF = bcm_pm_InterfaceCR4;                              \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_SR) {              \
            EPDM_IF = bcm_pm_InterfaceSR;                               \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_SR4) {             \
            EPDM_IF = bcm_pm_InterfaceSR4;                              \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_LR) {              \
            EPDM_IF = bcm_pm_InterfaceLR;                               \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_LR4) {             \
            EPDM_IF = bcm_pm_InterfaceLR4;                              \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_KR) {              \
            EPDM_IF = bcm_pm_InterfaceKR;                               \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_KR4) {             \
            EPDM_IF = bcm_pm_InterfaceKR4;                              \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_XFI) {             \
            EPDM_IF = bcm_pm_InterfaceXFI;                              \
        } else if (SAI_IF == SAI_PORT_INTERFACE_TYPE_XGMII) {           \
            EPDM_IF = bcm_pm_InterfaceXGMII;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_1000X) {      \
            EPDM_IF = bcm_pm_Interface1000X;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_SGMII) {      \
            EPDM_IF = bcm_pm_InterfaceSGMII;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_2500R) {      \
            EPDM_IF = bcm_pm_Interface2500R;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_2500X) {      \
            EPDM_IF = bcm_pm_Interface2500X;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_5000R) {      \
            EPDM_IF = bcm_pm_Interface5000R;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_5000X) {      \
            EPDM_IF = bcm_pm_Interface5000X;                            \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_USXGMII) {    \
            EPDM_IF = bcm_pm_InterfaceUSXGMII;                          \
        } else if (SAI_IF == BRCM_PAI_PORT_INTERFACE_TYPE_USXGMII_KR) { \
            EPDM_IF = bcm_pm_InterfaceKRUSXGMII;                        \
        } else {                                                        \
            EPDM_IF = bcm_pm_InterfaceKR;                               \
        }

#define BRCM_PAI_EPDM_TYPE_PAI_IF(EPDM_IF,PAI_IF)                  \
        if (EPDM_IF == bcm_pm_InterfaceCR) {                       \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_CR;                   \
        } else if (EPDM_IF == bcm_pm_InterfaceCR4) {               \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_CR4;                  \
        } else if (EPDM_IF == bcm_pm_InterfaceSR ) {               \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_SR;                   \
        } else if (EPDM_IF == bcm_pm_InterfaceSR4 ) {              \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_SR4;                  \
        } else if (EPDM_IF == bcm_pm_InterfaceLR ) {               \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_LR;                   \
        } else if (EPDM_IF == bcm_pm_InterfaceLR4 ) {              \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_LR4;                  \
        } else if (EPDM_IF == bcm_pm_InterfaceKR) {                \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_KR ;                  \
        } else if (EPDM_IF == bcm_pm_InterfaceKR4 ) {              \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_KR4 ;                 \
        } else if (EPDM_IF == bcm_pm_InterfaceXFI) {               \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_XFI;                  \
        } else if (EPDM_IF == bcm_pm_InterfaceXGMII ) {            \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_XGMII;                \
        } else if (EPDM_IF == bcm_pm_Interface1000X ) {            \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_1000X;           \
        } else if (EPDM_IF == bcm_pm_InterfaceSGMII ) {            \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_SGMII;           \
        } else if (EPDM_IF == bcm_pm_Interface2500R ) {            \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_2500R;           \
        } else if (EPDM_IF == bcm_pm_Interface2500X) {             \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_2500X;           \
        } else if (EPDM_IF == bcm_pm_Interface5000R ) {            \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_5000R;           \
        } else if (EPDM_IF == bcm_pm_Interface5000X ) {            \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_5000X;           \
        } else if (EPDM_IF == bcm_pm_InterfaceUSXGMII ) {          \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_USXGMII;         \
        } else if (EPDM_IF == bcm_pm_InterfaceKRUSXGMII ) {        \
            PAI_IF = BRCM_PAI_PORT_INTERFACE_TYPE_USXGMII_KR;      \
        } else {                                                   \
            PAI_IF = SAI_PORT_INTERFACE_TYPE_KR;                   \
        }

typedef struct brcm_pai_port_config_s {
    int speed;
    int pai_interface_type;
    int fec_mode;
    int break_out;
    unsigned int low_latency_variation;
} brcm_pai_port_config_t;

sai_status_t
brcm_epdm_err_to_pai(int rv);

sai_status_t brcm_pai_get_chip_name_id
             (int phy_id, char *chip_name, int *chip_id);

sai_status_t
brcm_pai_get_max_pkg_lanes(int phy_id, int if_side, uint32_t *no_of_lanes);

bool brcm_pai_api_is_initialized(void);

sai_status_t brcm_pai_epdm_mode_config_set
             (pai_port_info_t *port_info, bcm_plp_access_t *plp_info);

sai_status_t brcm_pai_epdm_loopback_set
             (pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
             sai_port_loopback_mode_t lb_mode);

sai_status_t
brcm_pai_epdm_fec_mode_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
                           sai_port_fec_mode_t fec_mode);

sai_status_t
brcm_pai_epdm_link_training_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
                                bool enable);

sai_status_t
brcm_pai_epdm_an_ability_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info);

sai_status_t
brcm_pai_epdm_an_enable_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info);

sai_status_t
brcm_pai_epdm_tx_rx_power_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
                                bool enable);

sai_status_t
brcm_pai_epdm_get_link_status(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                         unsigned int *link_status);

sai_status_t
brcm_pai_epdm_link_training_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
                                bool *enable);

sai_status_t
brcm_pai_epdm_loopback_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
                           unsigned int *lb_mode);

sai_status_t
brcm_pai_epdm_tx_rx_power_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, 
                                bool *enable);

sai_status_t 
brcm_pai_epdm_mode_config_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                                brcm_pai_port_config_t *port_config);
void print_stack_trace(void);

sai_status_t
brcm_pai_epdm_lanemap_set(char *chip_name, bcm_plp_access_t plp_info, unsigned int max_lanes,
                           unsigned int *tx_map, unsigned int *rx_map);

sai_status_t
brcm_pai_epdm_polarity_set(char *chip_name, bcm_plp_access_t plp_info,
                           unsigned int tx_polarity, unsigned int rx_polarity);

sai_status_t
brcm_pai_epdm_tx_fir_set_get(char *chip_name, bcm_plp_access_t *plp_info, uint32_t attr_count,
                             sai_attribute_t *attr_list, PAI_API_OPERATION_T set_get, uint32_t max_lanes);
sai_status_t
brcm_pai_epdm_get_fw_version(sai_object_id_t switch_id, sai_uint32_t *pai_fw_ver);

sai_status_t
brcm_pai_epdm_an_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                            unsigned int *an_enable, unsigned int *an_status);
sai_status_t
brcm_pai_epdm_link_training_status_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                                            brcm_pai_lt_status_t *lt_status);

sai_status_t
brcm_pai_epdm_prbs_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                                uint32_t poly, sai_uint32_t prbs_config);
sai_status_t
brcm_pai_epdm_prbs_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                                uint32_t tx_rx, brcm_pai_prbs_t *prbs);
sai_status_t
brcm_pai_epdm_prbs_status_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
                                brcm_pai_prbs_status_t *prbs_status);
sai_status_t
brcm_pai_epdm_fec_enable_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info);
sai_status_t
brcm_pai_epdm_fec_enable_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
    sai_uint32_t *fec_enable_get);
sai_status_t
brcm_pai_epdm_fec_counter_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info,
    PAI_FEC_COUNTER_TYPE_T fec_counter_type, unsigned int *counter);
int brcm_pai_macsec_user_mutex_lock(void* platform_ctxt);
int brcm_pai_macsec_user_mutex_unlock(void* platform_ctxt);
int brcm_pai_user_mutex_lock(unsigned int phy_id, void* platform_ctxt);
int brcm_pai_user_mutex_unlock(unsigned int phy_id, void* platform_ctxt);
sai_status_t
brcm_pai_ref_clk_to_epdm_ref_clk(sai_uint64_t pai_ref_clk, bcm_pm_ref_clk_t *epdm_ref_clk);
sai_status_t
brcm_epdm_ref_clk_to_pai_ref_clk(bcm_pm_ref_clk_t epdm_ref_clk, sai_uint64_t *pai_ref_clk);
sai_status_t
brcm_pai_epdm_synce_config_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, brcm_pai_synce_cfg_t synce_cfg);
sai_status_t
brcm_global_lane_to_local_lane(sai_object_id_t switch_id, bcm_plp_access_t *plp_info, uint32_t global_lane, uint32_t *local_lane);
sai_status_t
brcm_pai_epdm_failover_mode_set_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, PAI_API_OPERATION_T set_get, unsigned int *failover_mode);
sai_status_t
brcm_pai_epdm_failover_config_set_get(char *chip_name, bcm_plp_access_t plp_info, PAI_API_OPERATION_T set_get, unsigned int *failover_cfg);
sai_status_t
brcm_pai_epdm_cu_eee_config_set_get(char *chip_name, bcm_plp_access_t *plp_info, pai_port_info_t *port_info,
    PAI_API_OPERATION_T set_get, unsigned int ctrl_select_in, unsigned int *ctrl_value_inout);
sai_status_t
brcm_pai_epdm_cu_eee_stats_get_clear(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, PAI_API_OPERATION_T set_get,
    unsigned int ctrl_select_in, unsigned int *ctrl_value_inout);
sai_status_t
brcm_pai_epdm_cu_config_set_get(char *chip_name, bcm_plp_access_t *plp_info, pai_port_info_t *port_info,
    PAI_API_OPERATION_T set_get, unsigned int ctrl_select_in, unsigned int *ctrl_select_inout);
sai_status_t
brcm_pai_epdm_cu_an_ability_set(pai_port_info_t *port_info, bcm_plp_access_t *plp_info);
sai_status_t
brcm_pai_epdm_cu_power_set_get(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, PAI_API_OPERATION_T set_get);
#ifdef PAI_SYSLOG_ENABLE
uint8_t brcm_sai_to_syslog_log_level_get(uint8_t sai_level);
#endif
sai_status_t
brcm_pai_epdm_cu_cable_diag(pai_port_info_t *port_info, bcm_plp_access_t *plp_info, sai_attribute_t *attr_list);
sai_status_t brcm_pai_is_1G_phy(char *chip_name);
sai_status_t brcm_pai_is_xgbaset_phy(char *chip_name);
sai_status_t brcm_pai_get_part_name(int phy_id, char *chip_name, char *part_no);
int brcm_pai_count_set_bits(unsigned int no);

#endif /** __BRCM_PAI_COMMON_H_ */
