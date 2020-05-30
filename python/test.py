#!/usr/bin/python

from sairedis import *

args = dict()
args["SAI_SWITCH_ATTR_INIT_SWITCH"] = "false"

r = create_switch(args);
print r

swid = r["oid"]

r = get_switch_attribute(swid, "SAI_SWITCH_ATTR_PORT_LIST")
print r

args = dict()
args["SAI_VLAN_ATTR_VLAN_ID"] = "11"
r = create_vlan(swid, args)
print r

vlan = r["oid"]

r = set_vlan_attribute(vlan, "SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES", "32")
print r

r = remove_vlan(vlan)
print r
