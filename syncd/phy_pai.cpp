/*
 * $Copyright: (c) 2021 Pensando Systems.
 * Pensando Systems Proprietary and Confidential. All rights reserved.$
 $Id$
 */
#ifdef __cplusplus
extern "C"{
#endif 

#define PLP_BARCHETTA_SUPPORT 

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <syslog.h>
#include "sai.h"
#include "epdm.h"
#include <bcm/error.h>
#include <bcm/link.h>
#include <bcm/port.h>
#include <bcm/init.h>
#include "saitypes.h"

#define PAI_MAX_PHY                         4
#define PAI_MAX_PHY_RETIMER                 3

#define PAI_LIB_NAME                        "bin/libbrcm_pai.so"
#define TRUE                                1
#define FALSE                               0
#define NUM_PROFILES                        4
#define MAX_PROFILE_KEY_SIZE                64
#define MAX_PROFILE_VAL_SIZE                128
#define PLATFORM_CTXT                       0xF0F0F0F0F0F0F0F0
#define BCM_PHY_81381 "barchetta"

#define COUNTOF(ary)        ((int) (sizeof (ary) / sizeof ((ary)[0])))

#define PAI_LOG_INFO(format,...)                                        \
    do {                                                                \
        syslog(LOG_INFO, "%s " format "\n",__func__,##__VA_ARGS__);     \
    } while (0);


#define NUM_OF_PORTS						2
#define NUM_LANES_PER_PORT_SYS_SIDE			4
#define NUM_LANES_PER_PORT_LINE_SIDE		2
#define	SIZE_LANE_LIST_ARRAY_SYS_SIDE		8
#define	SIZE_LANE_LIST_ARRAY_LINE_SIDE		4
#define	LANES_SYS_SIDE						8
#define	LANES_LINE_SIDE						4
#define	GLOBAL_LANE_NUM_START_SYS_SIDE		8
#define	GLOBAL_LANE_NUM_START_LINE_SIDE		0
#define PORT_SPEED							100000
#define ENABLE								1
#define DISABLE								0

//Please use the appropriate phy_id based on the board type
int phy_id[PAI_MAX_PHY] = {0x0, 0x20, 0x40, 0x60};  /*For P0 board*/
//int phy_id[PAI_MAX_PHY] = {0x0, 0x2 0x4, 0x6};      /*For P1 board*/
static void *object_apis = NULL;
sai_switch_api_t *pai_switch_apis_ptr;
sai_port_api_t *pai_port_apis_ptr;


sai_status_t (*brcm_api_initialize)(uint64_t flags, const sai_service_method_table_t *services);
sai_status_t (*brcm_api_uninitialize)(void);
sai_status_t (*brcm_api_query) (sai_api_t api, void **api_method_table);
sai_object_id_t sys_port_id[PAI_MAX_PHY][NUM_OF_PORTS], line_port_id[PAI_MAX_PHY][NUM_OF_PORTS], port_conn_id[PAI_MAX_PHY][NUM_OF_PORTS];

/* Function to read data from MDIO interface */
sai_status_t mdio_read(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, uint32_t *data)
{
    int           bcm_ret;
    int           unit = 0;
    unsigned int  flags;
    unsigned int  c45_addr;
    unsigned char dev_addr = (reg_addr >> 16) & 0x1f;

    if (!platform_context) {
        return -1;
    }

    if (number_of_registers > 1) {
        printf("Multiple register reads are not supported, num_of_registers: %d\n", number_of_registers);
        return -1;
    }

    /* specify Clause 45 with MDIO address */
    flags = BCM_PORT_PHY_CLAUSE45 | BCM_PORT_PHY_NOMAP;
    /* encode the device address and register address */
    c45_addr = BCM_PORT_PHY_CLAUSE45_ADDR(dev_addr, reg_addr);

    *data   = 0xffffffff;
    bcm_ret = bcm_port_phy_get(unit, mdio_addr, flags, c45_addr, data);
    if (bcm_ret != BCM_E_NONE) {
        PAI_LOG_INFO( "MDIO_TRACE: READ Register Address: [0x%x] Data:[0x%x] MDIO Address: [0x%x] return [%d]\n", reg_addr, *data, mdio_addr, bcm_ret);
    }
    return bcm_ret;
}

/* Function to write data to MDIO interface */
sai_status_t mdio_write(uint64_t platform_context, uint32_t mdio_addr,
        uint32_t reg_addr, uint32_t number_of_registers, const uint32_t *data)
{
    int           bcm_ret;
    int           unit = 0;
    unsigned int  flags;
    unsigned int  c45_addr;
    unsigned char dev_addr = (reg_addr >> 16) & 0x1f;

    if (!platform_context) {
        return -1;
    }

    if (number_of_registers > 1) {
        printf("Multiple register reads are not supported, num_of_registers: %d\n", number_of_registers);
        return -1;
    }

    /* specify Clause 45 with MDIO address */
    flags = BCM_PORT_PHY_CLAUSE45 | BCM_PORT_PHY_NOMAP;
    /* encode the device address and register address */
    c45_addr = BCM_PORT_PHY_CLAUSE45_ADDR(dev_addr, reg_addr);

    bcm_ret = bcm_port_phy_set(unit, mdio_addr, flags, c45_addr, *data);
    if (bcm_ret != BCM_E_NONE) {
        PAI_LOG_INFO( "MDIO_TRACE: READ Register Address: [0x%x] Data:[0x%x] MDIO Address: [0x%x] return [%d]\n", reg_addr, *data, mdio_addr, bcm_ret);
    }
    return bcm_ret;
}

typedef struct _profile_kvp_s {
    char k[MAX_PROFILE_KEY_SIZE];
    char v[MAX_PROFILE_VAL_SIZE];
} profile_kvp_t;

static const profile_kvp_t profile_kvp_table[NUM_PROFILES][NUM_PROFILES] = {
    {
        { SAI_KEY_INIT_CONFIG_FILE, "config_81358.bcm" },
        { SAI_KEY_BOOT_TYPE, "0" },
    },
};

const char*
brcm_pai_profile_get_value(_In_ sai_switch_profile_id_t profile_id,
                  _In_ const char* variable);
int
brcm_pai_profile_get_next_value(_In_ sai_switch_profile_id_t profile_id,
                       _Out_ const char** variable,
                       _Out_ const char** value);


static const sai_service_method_table_t brcm_pai_services = {
    .profile_get_value = brcm_pai_profile_get_value,
    .profile_get_next_value = brcm_pai_profile_get_next_value
};

sai_attribute_t attr_set[PAI_MAX_PHY][10];

/*
* Get variable value given its name
*/
const char*
brcm_pai_profile_get_value(_In_ sai_switch_profile_id_t profile_id,
                  _In_ const char* variable)
{
    int i;

    for (i = 0; i<NUM_PROFILES; i++)
    {
        if (0 == strncmp(variable, profile_kvp_table[profile_id][i].k,
                         MAX_PROFILE_KEY_SIZE)) {
            return profile_kvp_table[profile_id][i].v;
        }
    }
    return NULL;
}

/*
* Enumerate all the K/V pairs in a profile.
* Pointer to NULL passed as variable restarts enumeration.
* Function returns 0 if next value exists, -1 at the end of the list.
*/
int
brcm_pai_profile_get_next_value(_In_ sai_switch_profile_id_t profile_id,
                       _Out_ const char** variable,
                       _Out_ const char** value)
{
    static int i[NUM_PROFILES] = { 0, 0, 0 };

    if (profile_id >= NUM_PROFILES) {
        return -1;
    }
    if (NULL == *variable)
    {
        i[profile_id] = 0;
        return 0;
    }
    if (variable)
    {
        *variable = profile_kvp_table[profile_id][i[profile_id]].k;
    }
    if (value)
    {
        *value = profile_kvp_table[profile_id][i[profile_id]].v;
    }
    if ((NUM_PROFILES-1) == i[profile_id])
    {
        return -1;
    }
    i[profile_id]++;

    return 0;
}

sai_status_t query_object_apis(sai_api_t api)
{
    sai_status_t rv;
    rv = brcm_api_query(api, &object_apis);
    return rv;
}

int app_serdes_op(sai_object_id_t *switch_id)
{
    sai_attribute_t sys_serdes_attr[4], line_serdes_attr[4];
    sai_object_id_t serdes_port[PAI_MAX_PHY][NUM_OF_PORTS];
    int phy_index = 0, rv = 0;
    int port_index = 0;

    /* Sys Side Create serdes*/
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++){
            sys_serdes_attr[0].id = SAI_PORT_SERDES_ATTR_PORT_ID;
            sys_serdes_attr[0].value.oid = sys_port_id[phy_index][port_index];
            rv = pai_port_apis_ptr->create_port_serdes(&serdes_port[phy_index][port_index], switch_id[phy_index],
                    1, sys_serdes_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("Sys serdes port failed return:%d\n", rv);
                return rv;
            }
            PAI_LOG_INFO("Serdes port:%lx\n", serdes_port[phy_index][port_index]);
        }
    }
    /* Set attribute*/
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index ++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++){
            sys_serdes_attr[0].id = SAI_PORT_SERDES_ATTR_PORT_ID;
            sys_serdes_attr[0].value.oid = sys_port_id[phy_index][port_index];
            sys_serdes_attr[1].id = SAI_PORT_SERDES_ATTR_TX_FIR_PRE1;
            sys_serdes_attr[1].value.u32 = -6;
            sys_serdes_attr[2].id = SAI_PORT_SERDES_ATTR_TX_FIR_MAIN;
            sys_serdes_attr[2].value.u32 = 69;
            sys_serdes_attr[3].id = SAI_PORT_SERDES_ATTR_TX_FIR_POST1;
            sys_serdes_attr[3].value.u32 = 0;
                
            rv = pai_port_apis_ptr->set_port_serdes_attribute(serdes_port[phy_index][port_index],
                    sys_serdes_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("Sys serdes port failed return:%d\n", rv);
                return rv;
            }
        }
    }

    /* Line Side Create serdes*/
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++){
            line_serdes_attr[0].id = SAI_PORT_SERDES_ATTR_PORT_ID;
            line_serdes_attr[0].value.oid = line_port_id[phy_index][port_index];
            rv = pai_port_apis_ptr->create_port_serdes(&serdes_port[phy_index][port_index], switch_id[phy_index],
                    1, line_serdes_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("Sys serdes port failed return:%d\n", rv);
                return rv;
            }
            PAI_LOG_INFO("Serdes port:%lx\n", serdes_port[phy_index][port_index]);
        }
    }
    /* Set attribute*/
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index ++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++){
            line_serdes_attr[0].id = SAI_PORT_SERDES_ATTR_PORT_ID;
            line_serdes_attr[0].value.oid = line_port_id[phy_index][port_index];
            line_serdes_attr[1].id = SAI_PORT_SERDES_ATTR_TX_FIR_PRE1;
            line_serdes_attr[1].value.u32 = -16;
            line_serdes_attr[2].id = SAI_PORT_SERDES_ATTR_TX_FIR_MAIN;
            line_serdes_attr[2].value.u32 = 132;
            line_serdes_attr[3].id = SAI_PORT_SERDES_ATTR_TX_FIR_POST1;
            line_serdes_attr[3].value.u32 = 0;
                
            rv = pai_port_apis_ptr->set_port_serdes_attribute(serdes_port[phy_index][port_index],
                 line_serdes_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("Sys serdes port failed return:%d\n", rv);
                return rv;
            }
        }
    }
    return rv;
}

int app_remove_port(sai_object_id_t *switch_id)
{
    int rv = 0, phy_index = 0, port_index = 0;
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index ++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++) {

			rv = pai_port_apis_ptr->remove_port_connector(port_conn_id[phy_index][port_index]);
			if (SAI_STATUS_SUCCESS != rv) {
				PAI_LOG_INFO("port connector removal failed return:%d\n", rv);
				return rv;
			}
			rv = pai_port_apis_ptr->remove_port(sys_port_id[phy_index][port_index]);
			if (SAI_STATUS_SUCCESS != rv) {
				PAI_LOG_INFO("Sys port removal failed return:%d\n", rv);
				return rv;
			}
			rv = pai_port_apis_ptr->remove_port(line_port_id[phy_index][port_index]);
			if (SAI_STATUS_SUCCESS != rv) {
				PAI_LOG_INFO("Line port removal failed return:%d\n", rv);
				return rv;
			}
		}
    }
    return rv;
}

int app_create_port(sai_object_id_t *switch_id) {
    sai_attribute_t sys_attr[6], line_attr[6], port_conn[NUM_OF_PORTS],get_attr[6];
    int port_index = 0;
    int rv = 0, phy_index = 0, i=0;
    unsigned int  sys_lane_list[SIZE_LANE_LIST_ARRAY_SYS_SIDE];
	unsigned int  line_lane_list[SIZE_LANE_LIST_ARRAY_LINE_SIDE];

    line_attr[0].id = sys_attr[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
    sys_attr[0].value.u32list.count = NUM_LANES_PER_PORT_SYS_SIDE;
    line_attr[0].value.u32list.count = NUM_LANES_PER_PORT_LINE_SIDE;
    sys_attr[0].value.u32list.list = sys_lane_list;
    line_attr[0].value.u32list.list = line_lane_list;

    line_attr[1].id = sys_attr[1].id = SAI_PORT_ATTR_SPEED;
    line_attr[1].value.u32= sys_attr[1].value.u32 = PORT_SPEED;

    line_attr[2].id = sys_attr[2].id = SAI_PORT_ATTR_INTERFACE_TYPE;
    //line_attr[2].value.u32 = SAI_PORT_INTERFACE_TYPE_CAUI; 
    //sys_attr[2].value.u32 = SAI_PORT_INTERFACE_TYPE_CAUI4;
    line_attr[2].value.u32 = sys_attr[2].value.u32 = SAI_PORT_INTERFACE_TYPE_KR;

    line_attr[3].id = sys_attr[3].id = SAI_PORT_ATTR_FEC_MODE;
    sys_attr[3].value.u32 = SAI_PORT_FEC_MODE_RS;
    line_attr[3].value.u32 = SAI_PORT_FEC_MODE_RS;
    
    line_attr[4].id = sys_attr[4].id = SAI_PORT_ATTR_LINK_TRAINING_ENABLE;
    sys_attr[4].value.booldata = DISABLE;
    line_attr[4].value.booldata = DISABLE;

    line_attr[5].id = sys_attr[5].id = SAI_PORT_ATTR_ADMIN_STATE;
    line_attr[5].value.booldata = sys_attr[5].value.booldata = ENABLE;

	PAI_LOG_INFO("Create Port\n");
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index ++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++) {
			for (i = 0; i < NUM_LANES_PER_PORT_SYS_SIDE; i++) {
				sys_lane_list[i] = GLOBAL_LANE_NUM_START_SYS_SIDE + (phy_index*16) + (port_index*NUM_LANES_PER_PORT_SYS_SIDE) + i;
				PAI_LOG_INFO("\n=====phy_index [%d], port_index[%d], sys_lane_list[%d] = %d======\n", phy_index, port_index, i, sys_lane_list[i]);
			}
			
			for (i = 0; i < NUM_LANES_PER_PORT_LINE_SIDE; i++){
				line_lane_list[i] = GLOBAL_LANE_NUM_START_LINE_SIDE + (phy_index*16) + (port_index*NUM_LANES_PER_PORT_LINE_SIDE) + i;
				PAI_LOG_INFO("\n=====phy_index [%d], port_index[%d], line_lane_list[%d] = %d======\n", phy_index, port_index, i, line_lane_list[i]);
			}

            rv = pai_port_apis_ptr->create_port(&sys_port_id[phy_index][port_index], switch_id[phy_index],COUNTOF(sys_attr), sys_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("Sys create port failed return:%d\n", rv);
                return rv;
            }
            PAI_LOG_INFO("Sys Create port: 0x%lx.\n", sys_port_id[phy_index][port_index]);
            rv = pai_port_apis_ptr->create_port(&line_port_id[phy_index][port_index], switch_id[phy_index], 
                    COUNTOF(line_attr),
                    line_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("Line create port failed return:%d\n", rv);
                return rv;
            }
            PAI_LOG_INFO("line Create port: 0x%lx.\n", line_port_id[phy_index][port_index]);
            port_conn[0].id = SAI_PORT_CONNECTOR_ATTR_SYSTEM_SIDE_PORT_ID; 
            port_conn[0].value.oid = sys_port_id[phy_index][port_index];
            port_conn[1].id = SAI_PORT_CONNECTOR_ATTR_LINE_SIDE_PORT_ID; 
            port_conn[1].value.oid = line_port_id[phy_index][port_index];
            rv = pai_port_apis_ptr->create_port_connector(&port_conn_id[phy_index][port_index], switch_id[phy_index], 
                    COUNTOF(port_conn),
                    port_conn);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("create_port_connector failed return:%d\n", rv);
                return rv;
            }
            PAI_LOG_INFO("PORT INDEX = %d at SYS side :: SPEED - %d :: INTERFACE_TYPE - %d :: FEC_MODE - %d\n", port_index, sys_attr[1].value.u32, sys_attr[2].value.u32, sys_attr[3].value.u32);
            PAI_LOG_INFO("PORT INDEX = %d at LINE side :: SPEED - %d :: INTERFACE_TYPE - %d :: FEC_MODE - %d\n", port_index, line_attr[1].value.u32, line_attr[2].value.u32, line_attr[3].value.u32);                

            PAI_LOG_INFO("Port connector ID for the port %d: 0x%lx.\n", port_index, port_conn_id[phy_index][port_index]);
        }
    }

    
    PAI_LOG_INFO("Get Port Attribute\n");
    for (phy_index = 0; phy_index < PAI_MAX_PHY; phy_index ++) {
        for (port_index = 0; port_index < NUM_OF_PORTS; port_index ++) {
            memset(get_attr, 0, sizeof(get_attr));
            get_attr[0].id = SAI_PORT_ATTR_OPER_SPEED;
            get_attr[1].id = SAI_PORT_ATTR_REFERENCE_CLOCK;
            get_attr[2].id = SAI_PORT_ATTR_FEC_MODE;
            get_attr[3].id = SAI_PORT_ATTR_INTERFACE_TYPE;
            get_attr[4].id = SAI_PORT_ATTR_MEDIA_TYPE;
            get_attr[5].id = SAI_PORT_ATTR_SPEED;

            rv = pai_port_apis_ptr->get_port_attribute(sys_port_id[phy_index][port_index], 6, get_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("get_port_attribute failed for port id 0x%lx return:%d\n", sys_port_id[phy_index][port_index], rv);
                return rv;
            }
    
            PAI_LOG_INFO("Sys Side:: PORT INDEX = %d PORT ID - 0x%lx : Port Operating speed - %d : RefClock - %lld : FEC_MODE - %d : INTERFACE_TYPE - %d : Media_type - %d : SPEED - %d\n", 
            port_index, sys_port_id[phy_index][port_index], get_attr[0].value.u32, (unsigned long long)get_attr[1].value.u64, get_attr[2].value.u32, get_attr[3].value.u32, get_attr[4].value.u32, get_attr[5].value.u32);
    
            memset(get_attr, 0, sizeof(get_attr));
            get_attr[0].id = SAI_PORT_ATTR_OPER_SPEED;
            get_attr[1].id = SAI_PORT_ATTR_REFERENCE_CLOCK;
            get_attr[2].id = SAI_PORT_ATTR_FEC_MODE;
            get_attr[3].id = SAI_PORT_ATTR_INTERFACE_TYPE;
            get_attr[4].id = SAI_PORT_ATTR_MEDIA_TYPE;
            get_attr[5].id = SAI_PORT_ATTR_SPEED;

            rv = pai_port_apis_ptr->get_port_attribute(line_port_id[phy_index][port_index], 6, get_attr);
            if (SAI_STATUS_SUCCESS != rv)  {
                PAI_LOG_INFO("get_port_attribute failed for port id 0x%lx return:%d\n", line_port_id[phy_index][port_index], rv);
                return rv;
            }
    
            PAI_LOG_INFO("Line Side:: PORT INDEX = %d PORT ID - 0x%lx : Port Operating speed - %d : RefClock - %lld : FEC_MODE - %d : INTERFACE_TYPE - %d : Media_type - %d : SPEED - %d\n", 
            port_index, line_port_id[phy_index][port_index], get_attr[0].value.u32, (unsigned long long)get_attr[1].value.u64, get_attr[2].value.u32, get_attr[3].value.u32, get_attr[4].value.u32, get_attr[5].value.u32);

        }
    }
    
    return rv;
}

int gearbox_init()
{
    sai_status_t rv = 0;
    uint64_t flags = 0;
    sai_object_id_t switch_id[PAI_MAX_PHY];
    int switch_index = 0;
    void *handle;
    sai_attribute_t sai_get_attr;
    uint32_t max_ports = 0;
    uint8_t count = 0;

    handle = dlopen (PAI_LIB_NAME, RTLD_LAZY);
    if (!handle) {
        fputs (dlerror(), stderr);
        return -1;
    }

    *(void**)(&brcm_api_initialize) = dlsym(handle, "sai_api_initialize");
    *(void**)(&brcm_api_uninitialize) = dlsym(handle, "sai_api_uninitialize");
    *(void**)(&brcm_api_query) = dlsym(handle, "sai_api_query");


    if (!brcm_api_initialize || !brcm_api_uninitialize || !brcm_api_query) {
        PAI_LOG_INFO("FAIL: Invalid API handles\n");
        return SAI_STATUS_ADDR_NOT_FOUND;
    }

    rv = brcm_api_initialize(flags, &brcm_pai_services);
    if (SAI_STATUS_SUCCESS != rv) {
        PAI_LOG_INFO("sai_api_initialize failed\n");
        return rv;
    }

    query_object_apis(SAI_API_SWITCH);
    pai_switch_apis_ptr = (sai_switch_api_t *)object_apis;
    query_object_apis(SAI_API_PORT);
    pai_port_apis_ptr = (sai_port_api_t *)object_apis;

    for (switch_index =0;switch_index < PAI_MAX_PHY; switch_index ++) {
        attr_set[switch_index][0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr_set[switch_index][0].value.booldata = TRUE;

        attr_set[switch_index][1].id = SAI_SWITCH_ATTR_SWITCH_PROFILE_ID;
        attr_set[switch_index][1].value.u32 = 0;

        attr_set[switch_index][2].id = SAI_SWITCH_ATTR_FIRMWARE_DOWNLOAD_BROADCAST;
        attr_set[switch_index][2].value.booldata = FALSE;

        attr_set[switch_index][3].id = SAI_SWITCH_ATTR_FIRMWARE_LOAD_METHOD;
        attr_set[switch_index][3].value.u32 = SAI_SWITCH_FIRMWARE_LOAD_METHOD_INTERNAL;

        attr_set[switch_index][4].id = SAI_SWITCH_ATTR_FIRMWARE_LOAD_TYPE;
        attr_set[switch_index][4].value.u32 = SAI_SWITCH_FIRMWARE_LOAD_TYPE_FORCE;

        attr_set[switch_index][5].id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;
        attr_set[switch_index][5].value.s8list.count=0x4;
        attr_set[switch_index][5].value.s8list.list=(int8_t *)&phy_id[switch_index];

        attr_set[switch_index][6].id = SAI_SWITCH_ATTR_REGISTER_READ;
        attr_set[switch_index][6].value.ptr = (void *) mdio_read;

        attr_set[switch_index][7].id = SAI_SWITCH_ATTR_REGISTER_WRITE;
        attr_set[switch_index][7].value.ptr = (void *) mdio_write;

        attr_set[switch_index][8].id = SAI_SWITCH_ATTR_PLATFROM_CONTEXT;
        attr_set[switch_index][8].value.u64 = PLATFORM_CTXT;
    }		

    if (pai_switch_apis_ptr->create_switch) {
        for (switch_index =0;switch_index < PAI_MAX_PHY; switch_index ++) {
            rv = pai_switch_apis_ptr->create_switch(&switch_id[switch_index], COUNTOF(attr_set[switch_index]),
                                                attr_set[switch_index]);
            if (SAI_STATUS_SUCCESS != rv) {
                PAI_LOG_INFO("Created switch failed: Error: %d\n", rv);
                return rv;
            }
            PAI_LOG_INFO("Switch Object created, id:0x%lx\n", switch_id[switch_index]);
        }
    }

    if (pai_port_apis_ptr->create_port) {
        rv = app_create_port(switch_id);    
        if (SAI_STATUS_SUCCESS != rv) {
            PAI_LOG_INFO("Created port failed: Error: %d\n", rv);
            //return rv;
        }

        rv = app_serdes_op(switch_id);
        if (SAI_STATUS_SUCCESS != rv) {
            PAI_LOG_INFO("serdes Op failed: Error: %d\n", rv);
            //return rv;
        }
    }

    if (pai_switch_apis_ptr->get_switch_attribute) {
        for (switch_index =0;switch_index < PAI_MAX_PHY; switch_index ++) {
            sai_get_attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
            rv = pai_switch_apis_ptr->get_switch_attribute(switch_id[switch_index], 1, &sai_get_attr);
            if (SAI_STATUS_SUCCESS != rv) {
                PAI_LOG_INFO("get switch attributes failed: Error: %d\n", rv);
                //return rv;
            }
            max_ports = sai_get_attr.value.u32;
            PAI_LOG_INFO("max_ports:%d\n", max_ports);
            sai_get_attr.id = SAI_SWITCH_ATTR_PORT_LIST;

            sai_get_attr.value.objlist.count = max_ports;
            sai_get_attr.value.objlist.list = (sai_object_id_t *) calloc(max_ports,
                sizeof(sai_object_id_t));
            rv = pai_switch_apis_ptr->get_switch_attribute(switch_id[switch_index], 1, &sai_get_attr);
            if (SAI_STATUS_SUCCESS != rv) {
                PAI_LOG_INFO("get switch attributes failed: Error: %d\n", rv);
                //return rv;
            }
            for(count = 0; count < sai_get_attr.value.objlist.count; count++) {
                PAI_LOG_INFO("Port list id %d is %lx \r\n", count, sai_get_attr.value.objlist.list[count]);
            }
            /* Get FW version */
            sai_get_attr.id = SAI_SWITCH_ATTR_FIRMWARE_MAJOR_VERSION;
            rv = pai_switch_apis_ptr->get_switch_attribute(switch_id[switch_index], 1, &sai_get_attr);
            if (SAI_STATUS_SUCCESS != rv) {
                PAI_LOG_INFO("get switch attributes failed: Error: %d\n", rv);
                //return rv;
            }
            PAI_LOG_INFO("FW Version: 0x%0x\n", sai_get_attr.value.u32);
            sai_get_attr.id = SAI_SWITCH_ATTR_PORT_CONNECTOR_LIST;
            sai_get_attr.value.objlist.count = max_ports;
            sai_get_attr.value.objlist.list = (sai_object_id_t *) calloc(max_ports,
                sizeof(sai_object_id_t));
            rv = pai_switch_apis_ptr->get_switch_attribute(switch_id[switch_index], 1, &sai_get_attr);
            if (SAI_STATUS_SUCCESS != rv) {
                PAI_LOG_INFO("get switch attributes failed: Error: %d\n", rv);
                //return rv;
            }
            for(count = 0; count < sai_get_attr.value.objlist.count; count++) {
                PAI_LOG_INFO("Port Connector list id %d is %lx \r\n", count, sai_get_attr.value.objlist.list[count]);
            }
        }
    }
	
#ifdef PHY_UNINIT_EN
    if (pai_port_apis_ptr->remove_port && pai_port_apis_ptr->remove_port_connector) {
        rv = app_remove_port(switch_id);
        if (SAI_STATUS_SUCCESS != rv) {
            PAI_LOG_INFO("port removal failed: Error: %d\n", rv);
            //return rv;
        }
    }
    if (pai_switch_apis_ptr->remove_switch) {
        for (switch_index=0; switch_index < PAI_MAX_PHY; switch_index++) {
            rv = pai_switch_apis_ptr->remove_switch(switch_id[switch_index]);
            if (SAI_STATUS_SUCCESS != rv) {
                PAI_LOG_INFO("Remove switch failed: Error: %d\n", rv);
                //return rv;
            }
        }
    }
#endif
	
    rv = brcm_api_uninitialize();
    if (SAI_STATUS_SUCCESS != rv) {
        PAI_LOG_INFO("sai_api_uninitialize failed\n");
    }
    return 0;
}

#define CLK_MODE_REC_CLK                                                      \
    0 /* Clock mode: Clock source as Recovered clock (lane-0 is used as clock \
         source) */
#define LL_MODE_DIS_BOTH_PATH \
    0x0 /* Low-latency mode: Disable at both Ingress and Egress paths */
#define LL_MODE_EN_INGR_PATH \
    0x1 /* Low-latency mode: Enable at Ingress and disable at Egress path */
#define LL_MODE_EN_EGR_PATH \
    0x2 /* Low-latency mode: Enable at Egress and disable at Ingress path */
#define LL_MODE_EN_BOTH_PATH \
    0x3 /* Low-latency mode: Enable at both Ingress and Egress paths */
#define PHY_RETIMER_PORT_COUNT 2 /* Ports per retimer device */

bcm_plp_logical_lane_map_t
    sys_logical_lane_retimer[PAI_MAX_PHY_RETIMER] = {
        /* num_of_lanes, rx_lane_list, tx_lane_list */
        {8, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}},
        {8, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}},
        {8, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}}};
bcm_plp_logical_lane_map_t
    line_logical_lane_retimer[PAI_MAX_PHY_RETIMER] = {
        /* num_of_lanes, rx_lane_list, tx_lane_list */
        {8, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}},
        {8, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}},
        {8, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}}};
unsigned int sys_lane_map_retimer[PHY_RETIMER_PORT_COUNT] = {0x0f, 0xf0};
unsigned int line_lane_map_retimer[PHY_RETIMER_PORT_COUNT] = {0x0f,
                                                                     0xf0};
int phy_addr_retimer[PAI_MAX_PHY_RETIMER] = {0x100, 0x120, 0x140};
unsigned int line_rx_pol[PAI_MAX_PHY_RETIMER] = {0x00, 0x00, 0x00};
unsigned int line_tx_pol[PAI_MAX_PHY_RETIMER] = {0x00, 0x00, 0x00};
unsigned int sys_rx_pol[PAI_MAX_PHY_RETIMER] = {0x00, 0xAD, 0xCD};
unsigned int sys_tx_pol[PAI_MAX_PHY_RETIMER] = {0x00, 0xAA, 0x05};

int
phy_mdio_read(void *user_acc, unsigned int mdio_addr, unsigned int reg_addr,
              unsigned int *data) {
    int           bcm_ret;
    int           unit = 0;
    unsigned int  flags;
    unsigned int  c45_addr;
    unsigned char dev_addr = (reg_addr >> 16) & 0x1f;

    /* specify Clause 45 with MDIO address */
    flags = BCM_PORT_PHY_CLAUSE45 | BCM_PORT_PHY_NOMAP;
    /* encode the device address and register address */
    c45_addr = BCM_PORT_PHY_CLAUSE45_ADDR(dev_addr, reg_addr);

    *data   = 0xffffffff;
    bcm_ret = bcm_port_phy_get(unit, mdio_addr, flags, c45_addr, data);
    if (bcm_ret != BCM_E_NONE) {
        PAI_LOG_INFO( "MDIO_TRACE: READ Register Address: [0x%x] Data:[0x%x] MDIO Address: [0x%x] return [%d]\n", reg_addr, *data, mdio_addr, bcm_ret);
    }
    return bcm_ret;
}


int
phy_mdio_write(void *user_acc, unsigned int mdio_addr, unsigned int reg_addr,
               unsigned int data) {
    int           bcm_ret;
    int           unit = 0;
    unsigned int  flags;
    unsigned int  c45_addr;
    unsigned char dev_addr = (reg_addr >> 16) & 0x1f;

    /* specify Clause 45 with MDIO address */
    flags = BCM_PORT_PHY_CLAUSE45 | BCM_PORT_PHY_NOMAP;
    /* encode the device address and register address */
    c45_addr = BCM_PORT_PHY_CLAUSE45_ADDR(dev_addr, reg_addr);

    bcm_ret = bcm_port_phy_set(unit, mdio_addr, flags, c45_addr, data);
    if (bcm_ret != BCM_E_NONE) {
        PAI_LOG_INFO( "MDIO_TRACE: READ Register Address: [0x%x] Data:[0x%x] MDIO Address: [0x%x] return [%d]\n", reg_addr, data, mdio_addr, bcm_ret);
    }
    return bcm_ret;
}

/* 100G NRZ (4x25G) Retimer */
int
retimer_init() {
    int                                  rv     = 0;
    int                                  p_ctxt = 5;
    bcm_plp_access_t                     phy_info;
    int                                  phy_id_retimer;
    int                                  lane_map_index;
    unsigned int                         sys_lane_map;
    unsigned int                         line_lane_map;
    unsigned int                         fw_ver;
    unsigned int                         fw_crc;
    unsigned int                         phy_rev;
    bcm_plp_barchetta_device_aux_modes_t aux_mode_sys;
    bcm_plp_barchetta_device_aux_modes_t aux_mode_line;
    bcm_plp_firmware_load_type_t         firmware_load_type;
    int                                  port_speed = 100000;

    /* Clear the PHY info and aux mode  structure */
    memset(&phy_info, 0, sizeof(bcm_plp_access_t));
    memset(&aux_mode_sys, 0, sizeof(bcm_plp_barchetta_device_aux_modes_t));
    memset(&aux_mode_line, 0, sizeof(bcm_plp_barchetta_device_aux_modes_t));
    memset(&firmware_load_type, 0, sizeof(bcm_plp_firmware_load_type_t));
    phy_info.platform_ctxt = &p_ctxt;

    /* Initialize the PHYs and download firmware */
    firmware_load_type.firmware_load_method = bcmpmFirmwareLoadMethodInternal;
    firmware_load_type.force_load_method    = bcmpmFirmwareLoadForce;
#if 0
    /* download firmware for each retimer (no broadcast) */
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
		    phy_id_retimer++) {
	    phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];
	    rv = bcm_plp_init_fw_bcast(BCM_PHY_81381, phy_info, phy_mdio_read,
			    phy_mdio_write, &firmware_load_type,
			    bcmpmFirmwareBroadcastNone);
	    if (rv != BCM_PM_IF_SUCCESS) {
		    PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareCoreReset failed rv %d",
				    phy_info.phy_addr, rv);
		    return -1;
	    } else {
		    PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareCoreReset success",
				    phy_info.phy_addr);
	    }
    }
#endif
    /** Reset the core for all phy id in mdio bus **/
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];
        if ((rv = bcm_plp_init_fw_bcast(
                 BCM_PHY_81381, phy_info, phy_mdio_read, phy_mdio_write,
                 &firmware_load_type, bcmpmFirmwareBroadcastCoreReset)) != 0) {
            PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareCoreReset failed rv %d",
                     phy_info.phy_addr, rv);
            return -1;
        } else {
            PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareCoreReset success",
                      phy_info.phy_addr);
        }
    }
    /** Enable the broadcast for all phy id in mdio bus **/
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];
        if ((rv = bcm_plp_init_fw_bcast(BCM_PHY_81381, phy_info, phy_mdio_read,
                                        phy_mdio_write, &firmware_load_type,
                                        bcmpmFirmwareBroadcastEnable)) != 0) {
            PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareEnable failed rv %d",
                     phy_info.phy_addr, rv);
            return -1;
        } else {
            PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareEnable success",
                      phy_info.phy_addr);
        }
    }

    /* Download firmware only on the first phy on the bus, it will broadcast
     * to all devices on the mdio bus */
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];
	if ((rv = bcm_plp_init_fw_bcast(BCM_PHY_81381, phy_info, phy_mdio_read,
					phy_mdio_write, &firmware_load_type,
					bcmpmFirmwareBroadcastFirmwareExecute)) !=
			0) {
		PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareExecute failed rv %d",
				phy_info.phy_addr, rv);
		return -1;
	} else {
		PAI_LOG_INFO("Phy 0x%x BroadcastFirmwareExecute success",
				phy_info.phy_addr);
	}
    }

    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];

        /* Read firmware info */
        rv = bcm_plp_firmware_info_get(BCM_PHY_81381, phy_info, &fw_ver,
                                       &fw_crc);
        if (rv != BCM_PM_IF_SUCCESS) {
            PAI_LOG_INFO("Phy 0x%x get firmware info failed rv %d",
                     phy_info.phy_addr, rv);
        } else {
            PAI_LOG_INFO("Phy 0x%x FW version 0x%x, FW CRC 0x%x",
                      phy_info.phy_addr, fw_ver, fw_crc);
        }

        /* Read PHY revision */
        rv = bcm_plp_rev_id(BCM_PHY_81381, phy_info, &phy_rev);
        if (rv != BCM_PM_IF_SUCCESS) {
            PAI_LOG_INFO("Phy 0x%x get rev_id failed rv %d", phy_info.phy_addr, rv);
        } else {
            PAI_LOG_INFO("Phy 0x%x rev_id 0x%x", phy_info.phy_addr, phy_rev);
        }
    }

    /* Setup logical lane-map for Tx/Rx at System and Line side */
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];

        /* system side lane mapping is already set on TD3 */
        /* Set logical lane-map at System side */
        phy_info.if_side = BCM_SYSTEM_SIDE;
        rv               = bcm_plp_logical_lane_set(
            BCM_PHY_81381, phy_info,
            sys_logical_lane_retimer[phy_id_retimer]);
        if (rv != BCM_PM_IF_SUCCESS) {
            PAI_LOG_INFO("Phy 0x%x set logical lane map system side failed rv %d",
                     phy_info.phy_addr, rv);
            return -1;
        }

        /* Set logical lane-map at Line side */
        phy_info.if_side = BCM_LINE_SIDE;
        rv               = bcm_plp_logical_lane_set(
            BCM_PHY_81381, phy_info,
            line_logical_lane_retimer[phy_id_retimer]);
        if (rv != BCM_PM_IF_SUCCESS) {
            PAI_LOG_INFO("Phy 0x%x set logical lane map line side failed rv %d",
                     phy_info.phy_addr, rv);
            return -1;
        }
    }

    /* Configure Tx/Rx polarity */
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];

        /* system side polarity flips are already set on TD3 */
        /* Set Tx/Rx polarity at System side */
        phy_info.if_side  = BCM_SYSTEM_SIDE;
        phy_info.lane_map = 0xFF;
        rv = bcm_plp_polarity_set(BCM_PHY_81381, phy_info, sys_tx_pol[phy_id_retimer],
                                  sys_rx_pol[phy_id_retimer]);
        if (rv != BCM_PM_IF_SUCCESS) {
            PAI_LOG_INFO("Phy 0x%x set tx/rx polarity system side failed rv %d",
                     phy_info.phy_addr, rv);
            return -1;
        }

        /* Set Tx/Rx polarity at Line side */
        phy_info.if_side  = BCM_LINE_SIDE;
        phy_info.lane_map = 0xFF;
        rv = bcm_plp_polarity_set(BCM_PHY_81381, phy_info, line_tx_pol[phy_id_retimer],
                                  line_rx_pol[phy_id_retimer]);
        if (rv != BCM_PM_IF_SUCCESS) {
            PAI_LOG_INFO("Phy 0x%x set tx/rx polarity line side failed rv %d",
                     phy_info.phy_addr, rv);
            return -1;
        }
    }

    /* Configure operating mode parameters */
    for (phy_id_retimer = 0; phy_id_retimer < PAI_MAX_PHY_RETIMER;
         phy_id_retimer++) {
        phy_info.phy_addr = phy_addr_retimer[phy_id_retimer];
        for (lane_map_index = 0; lane_map_index < PHY_RETIMER_PORT_COUNT;
             lane_map_index++) {
            sys_lane_map  = sys_lane_map_retimer[lane_map_index];
            line_lane_map = line_lane_map_retimer[lane_map_index];

            /* Set mode configuration at System side */
            phy_info.lane_map            = sys_lane_map;
            phy_info.if_side             = BCM_SYSTEM_SIDE;
            aux_mode_sys.lane_data_rate  = bcmpLplaneDataRate_25P78125G;
            aux_mode_sys.modulation_mode = bcmplpModulationNRZ;
            aux_mode_sys.clock_mode      = CLK_MODE_REC_CLK;
            aux_mode_sys.ll_mode =
                LL_MODE_EN_BOTH_PATH; /* Must be 0x3 for non-FEC */
            aux_mode_sys.failover_lane_map = 0x0;
            rv                             = bcm_plp_mode_config_set(
                BCM_PHY_81381, phy_info, port_speed, bcm_pm_InterfaceKR,
                bcm_pm_RefClk156Mhz, 0x0, (void *)&aux_mode_sys);
            if (rv != BCM_PM_IF_SUCCESS) {
                PAI_LOG_INFO(
                    "Phy 0x%x mode config failed on %s side, "
                    "lane_map 0x%x port_speed %d "
                    "lane_speed %d modulation_mode %d rv %d",
                    phy_info.phy_addr,
                    (phy_info.if_side == BCM_SYSTEM_SIDE) ? "sys" : "line",
                    phy_info.lane_map, port_speed, aux_mode_sys.lane_data_rate,
                    aux_mode_sys.modulation_mode, rv);
                return -1;
            }

            /* Set mode configuration at Line side */
            phy_info.lane_map             = line_lane_map;
            phy_info.if_side              = BCM_LINE_SIDE;
            aux_mode_line.lane_data_rate  = bcmpLplaneDataRate_25P78125G;
            aux_mode_line.modulation_mode = bcmplpModulationNRZ;
            aux_mode_line.clock_mode      = CLK_MODE_REC_CLK;
            aux_mode_line.ll_mode =
                LL_MODE_EN_BOTH_PATH; /* Must be 0x3 for non-FEC */
            aux_mode_line.failover_lane_map = 0x0;
            rv                              = bcm_plp_mode_config_set(
                BCM_PHY_81381, phy_info, port_speed, bcm_pm_InterfaceKR,
                bcm_pm_RefClk156Mhz, 0x0, (void *)&aux_mode_line);
            if (rv != BCM_PM_IF_SUCCESS) {
                PAI_LOG_INFO(
                    "Phy 0x%x mode config failed on %s side, "
                    "lane_map 0x%x port_speed %d "
                    "lane_speed %d modulation_mode %d rv %d",
                    phy_info.phy_addr,
                    (phy_info.if_side == BCM_SYSTEM_SIDE) ? "sys" : "line",
                    phy_info.lane_map, port_speed, aux_mode_line.lane_data_rate,
                    aux_mode_line.modulation_mode, rv);
                return -1;
            }
        }
    }

    return rv;
}

#ifdef __cplusplus
}
#endif
