#ifndef __HPE_CUSTOM_CMDS_H__
#define __HPE_CUSTOM_CMDS_H__

// Structures
typedef struct tx_eq_parameters {
    int      unit;
    int      port;
    uint32_t pre_cursor_tap;
    uint32_t main_cursor_tap;
    uint32_t post_cursor1_tap;
    uint32_t post_cursor2_tap;
    uint32_t post_cursor3_tap;
    uint32_t amp_ctrl;
    int      phy_ext_line;
} tx_eq_parameters_s;

// Functions
int register_hpe_port_status_handler(int (*callback)(void));
int register_hpe_virtual_vlan_show_handler(int (*callback)(void));

int register_mavic_emi_test_normal_handler(int (*callback)(void));
int register_mavic_emi_test_sfi_handler(int (*callback)(void));
int register_mavic_emi_test_xfi_handler(int (*callback)(void));

int register_mavic_phy_sfi_enable_handler(int (*callback)(int, int, int));
int register_mavic_phy_xfi_enable_handler(int (*callback)(int, int, int));
int register_macsec_phy_configure_handler(int (*callback)(uint32_t, char *,
                                                          uint32_t));

// Snake tests
int register_mavic_ff_snake_test_handler(int (*callback)(int, uint32_t));
int register_mavic_vlan_snake_test_handler(int (*callback)(void));

// Vlan functions
int register_mavic_vlan_create_handler(int (*callback)(int, int));
int register_mavic_vlan_destroy_handler(int (*callback)(int, int));
int register_mavic_vlan_add_port_handler(int (*callback)(int, int, int));
int register_mavic_vlan_remove_port_handler(int (*callback)(int, int, int));
int register_mavic_vlan_enable_flooding_handler(int (*callback)(int, int));

// Link Status
int register_mavic_link_status_monitor_handler(int (*callback)(int));

// Tx EQ
int register_mavic_configure_internal_tx_eq_handler(
    int (*callback)(tx_eq_parameters_s *tx_eq_parameters));

int register_mavic_configure_external_tx_eq_handler(
    int (*callback)(tx_eq_parameters_s *tx_eq_parameters));

int register_mavic_internal_tx_eq_get_handler(int (*callback)(int, int));

int register_mavic_external_tx_eq_get_handler(int (*callback)(int, int, int));

// KBP/PHY mux select
int register_hpe_kbp_phy_mux_select_handler(int (*callback)(int, int));

// Run infinite KBP mem test
int register_hpe_kbp_infinite_mem_test_handler(int (*callback)(void));

// Stop infinite KBP mem test
int register_hpe_kbp_mem_test_stop_handler(int (*callback)(void));

// Phy temp read
int register_mavic_external_phy_temp_read_handler(int (*callback)(int, int));

// Ext phy dump
int register_hpe_ext_phy_dump_handler(int (*callback)(int, int, int));

// Ext phy tracing
int register_hpe_ext_phy_trace_enable_handler(int (*callback)(bool));
int register_hpe_ext_phy_trace_disable_handler(int (*callback)(bool));

#endif /* __HPE_CUSTOM_CMDS_H__ */
