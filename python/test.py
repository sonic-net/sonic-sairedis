#!/usr/bin/python

from sairedis import *

profileMap = dict()

profileMap["SAI_WARM_BOOT_READ_FILE"] = "./sai_warmboot.bin"
profileMap["SAI_WARM_BOOT_WRITE_FILE"] = "./sai_warmboot.bin"

api_initialize(profileMap)

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT", "3000")
print "set timeout: " + str(r)

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD", "INIT_VIEW")
print "init view: " + str(r)

args = dict()
args["SAI_SWITCH_ATTR_INIT_SWITCH"] = "true"

r = create_switch(args)
print "create switch: " + str(r)

swid = r["oid"]

r = set_switch_attribute("oid:0x0", "SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD", "APPLY_VIEW")
print "apply view: " + str(r)

r = get_switch_attribute(swid, "SAI_SWITCH_ATTR_PORT_LIST")
print "get port list: " + str(r)

args = dict()
args["SAI_VLAN_ATTR_VLAN_ID"] = "11"
r = create_vlan(swid, args)
print "create vlan: " + str(r)

vlan = r["oid"]

r = set_vlan_attribute(vlan, "SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES", "32")
print "set vlan attribute: " + str(r)

r = remove_vlan(vlan)
print "remove vlan: " + str(r)

api_uninitialize()
