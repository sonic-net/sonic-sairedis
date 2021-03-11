#!/usr/bin/python

# redis-cli FLUSHALL
# run syncd: syncd -SUu -z redis_sync -p vsprofile.ini

from sairedis import *

profileMap = dict()

profileMap["SAI_WARM_BOOT_READ_FILE"] = "./sai_warmboot.bin"
profileMap["SAI_WARM_BOOT_WRITE_FILE"] = "./sai_warmboot.bin"

r = api_initialize(profileMap)
print "initialize: " + str(r)

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT", "3000")
print "set timeout: " + str(r)

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE", "redis_sync")
print "set communication mode: " + str(r)

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD", "INIT_VIEW")
print "init view: " + str(r)

args = dict()
args["SAI_SWITCH_ATTR_INIT_SWITCH"] = "true"
args["SAI_SWITCH_ATTR_SRC_MAC_ADDRESS"] = "90:B1:1C:F4:A8:53"

r = create_switch(args)
print "create switch: " + str(r)

swid = r["oid"]

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD", "APPLY_VIEW")
print "apply view: " + str(r)

r = get_switch_attribute(swid, "SAI_SWITCH_ATTR_PORT_LIST")
print "get port list: " + str(r)

portlist = r["SAI_SWITCH_ATTR_PORT_LIST"]

r = get_switch_attribute(swid, "SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID")
print "get default virtual router: " + str(r)

vrid = r["SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID"]

args = dict()
r = create_lag(swid, args)
print "create lag: " + str(r)

lagid = r["oid"]

args = dict()
args["SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID"] = vrid
args["SAI_ROUTER_INTERFACE_ATTR_TYPE"] = "SAI_ROUTER_INTERFACE_TYPE_PORT"
args["SAI_ROUTER_INTERFACE_ATTR_PORT_ID"] = portlist[0]
r = create_router_interface(swid, args)
print "create router interface: " + str(r)

rifid = r["oid"]

args = dict()
args["SAI_NEXT_HOP_ATTR_TYPE"] = "SAI_NEXT_HOP_TYPE_IP"
args["SAI_NEXT_HOP_ATTR_IP"] = "10.0.0.1"
args["SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID"] = rifid
r = create_next_hop(swid, args)
print "create next hop: " + str(r)

nexthopid = r["oid"]

re = dict()
re["destination"] = "0.0.0.0/0"
re["switch_id"] = swid
re["vr_id"] = vrid

args = dict()
args["SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION"] = "SAI_PACKET_ACTION_DROP"
r = create_route_entry(re, args)
print "create default route entry: " + str(r)

re = dict()
re["destination"] = "100.1.0.1/32"
re["switch_id"] = swid
re["vr_id"] = vrid

args = dict()
args["SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID"] = nexthopid
r = create_route_entry(re, args)
print "create route entry: " + str(r)

args = dict()
args["SAI_VLAN_ATTR_VLAN_ID"] = "11"
r = create_vlan(swid, args)
print "create vlan: " + str(r)

vlan = r["oid"]

fe = dict()
fe["mac_address"] = "FE:54:00:B3:06:8C"
fe["switch_id"] = swid
fe["bv_id"] = vlan

args = dict()
args["SAI_FDB_ENTRY_ATTR_TYPE"] = "SAI_FDB_ENTRY_TYPE_DYNAMIC"
args["SAI_FDB_ENTRY_ATTR_PACKET_ACTION"] = "SAI_PACKET_ACTION_FORWARD"
r = create_fdb_entry(fe, args)
print "create fdb entry: " + str(r)

r = remove_fdb_entry(fe)
print "remove fdb entry: " + str(r)

r = set_vlan_attribute(vlan, "SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES", "32")
print "set vlan attribute: " + str(r)

r = remove_vlan(vlan)
print "remove vlan: " + str(r)

r = set_route_entry_attribute(re, "SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DROP")
print "set route entry: " + str(r)

r = get_route_entry_attribute(re, "SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION")
print "get route entry packet action: " + str(r)

r = remove_route_entry(re)
print "remove route entry: " + str(r)

r = api_uninitialize()
print "uninitialize: " + str(r)

