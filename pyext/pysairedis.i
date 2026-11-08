%module pysairedis
%include "cpointer.i"
%include "carrays.i"

// These objects cause issues on Buster because of the function pointers
%ignore _sai_struct_member_info_t;
%ignore _sai_object_type_info_t;

%{
#pragma GCC optimize("no-var-tracking-assignments")

#include "pysairedis.h"

extern "C"{
#include "sai.h"
#include "saiextensions.h"
#include "saimetadata.h"
}

#include "sairedis.h"

%}

%include "std_string.i"
%include "std_map.i"

namespace std {
    %template(map_string_string) map<string, string>;
}

%include "pysairedis.h"

%include "saitypes.h"
%include "sai.h"
%include "saiextensions.h"

%include "../SAI/meta/saiswig.i"

%include "sairedis.h"

// helper functions

%{
sai_mac_t* sai_mac_t_from_string(const std::string& s);
sai_ip_address_t* sai_ip_address_t_from_string(const std::string& s);
sai_ip_prefix_t* sai_ip_prefix_t_from_string(const std::string& s);
%}

sai_mac_t* sai_mac_t_from_string(const std::string& s);
sai_ip_address_t* sai_ip_address_t_from_string(const std::string& s);
sai_ip_prefix_t* sai_ip_prefix_t_from_string(const std::string& s);

%newobject sai_mac_t_from_string;
%newobject sai_ip_address_t_from_string;
%newobject sai_ip_prefix_t_from_string;

// array functions

%include <stdint.i>

%array_functions(uint32_t, uint32_t_arr);
%pointer_functions(uint32_t, uint32_t_p);

%array_functions(sai_object_id_t, sai_object_id_t_arr);
%pointer_functions(sai_object_id_t, sai_object_id_t_p);

%array_functions(sai_attribute_t, sai_attribute_t_arr);
%pointer_functions(sai_attribute_t, sai_attribute_t_p);

%array_functions(sai_bfd_session_state_notification_t, sai_bfd_session_state_notification_t_arr);
%pointer_functions(sai_bfd_session_state_notification_t, sai_bfd_session_state_notification_t_p);
%array_functions(sai_icmp_echo_session_state_notification_t, sai_icmp_echo_session_state_notification_t_arr);
%pointer_functions(sai_icmp_echo_session_state_notification_t, sai_icmp_echo_session_state_notification_t_p);

%array_functions(sai_ha_set_event_data_t, sai_ha_set_event_data_t_arr);
%pointer_functions(sai_ha_set_event_data_t, sai_ha_set_event_data_t_p);
%array_functions(sai_ha_scope_event_data_t, sai_ha_scope_event_data_t_arr);
%pointer_functions(sai_ha_scope_event_data_t, sai_ha_scope_event_data_t_p);
%array_functions(sai_flow_bulk_get_session_event_data_t, sai_flow_bulk_get_session_event_data_t_arr);
%pointer_functions(sai_flow_bulk_get_session_event_data_t, sai_flow_bulk_get_session_event_data_t_p);
%array_functions(sai_fdb_event_notification_data_t, sai_fdb_event_notification_data_t_arr);
%pointer_functions(sai_fdb_event_notification_data_t, sai_fdb_event_notification_data_t_p);
%array_functions(sai_port_oper_status_notification_t, sai_port_oper_status_notification_t_arr);
%pointer_functions(sai_port_oper_status_notification_t, sai_port_oper_status_notification_t_p);
%array_functions(sai_queue_deadlock_notification_data_t, sai_queue_deadlock_notification_data_t_arr);
%pointer_functions(sai_queue_deadlock_notification_data_t, sai_queue_deadlock_notification_data_t_p);

%{
PyObject *py_convert_sai_fdb_event_notification_data_t_to_PyObject(const sai_fdb_event_notification_data_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_fdb_event_notification_data_t, 0 | 0); }
PyObject *py_convert_sai_bfd_session_state_notification_t_to_PyObject(const sai_bfd_session_state_notification_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_bfd_session_state_notification_t, 0 | 0); }
PyObject *py_convert_sai_icmp_echo_session_state_notification_t_to_PyObject(const sai_icmp_echo_session_state_notification_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_icmp_echo_session_state_notification_t, 0 | 0); }
PyObject *py_convert_sai_ha_set_event_data_t_to_PyObject(const sai_ha_set_event_data_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_ha_set_event_data_t, 0 | 0); }
PyObject *py_convert_sai_ha_scope_event_data_t_to_PyObject(const sai_ha_scope_event_data_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_ha_scope_event_data_t, 0 | 0); }
PyObject *py_convert_sai_flow_bulk_get_session_event_data_t_to_PyObject(const sai_flow_bulk_get_session_event_data_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_flow_bulk_get_session_event_data_t, 0 | 0); }
PyObject *py_convert_sai_port_oper_status_notification_t_to_PyObject(const sai_port_oper_status_notification_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_port_oper_status_notification_t, 0 | 0); }
PyObject *py_convert_sai_queue_deadlock_notification_data_t_to_PyObject(const sai_queue_deadlock_notification_data_t*ntf)
{ return SWIG_NewPointerObj((void*)ntf, SWIGTYPE_p__sai_queue_deadlock_notification_data_t, 0 | 0); }
%}


