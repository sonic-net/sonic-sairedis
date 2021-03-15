%module pysairedis

%include "cpointer.i"
%include "carrays.i"

%{
#include "pysairedis.h"

extern "C"{
#include "sai.h"
#include "getapi.h"
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

%include "getapi.h"

%include "switch.h"
%include "lag.h"
%include "routerinterface.h"
%include "nexthop.h"
%include "route.h"
%include "vlan.h"
%include "fdb.h"

%ignore sai_switch_api_t;
%ignore sai_lag_api_t;
%ignore sai_router_interface_api_t;
%ignore sai_next_hop_api_t;
%ignore sai_route_api_t;
%ignore sai_vlan_api_t;
%ignore sai_fdb_api_t;

%include "saiswitch.h"
%include "sailag.h"
%include "sairouterinterface.h"
%include "sainexthop.h"
%include "sairoute.h"
%include "saivlan.h"
%include "saifdb.h"


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

//%array_class(sai_attribute_t, sai_attribute_tArray);

