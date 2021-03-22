#!/usr/bin/python

# redis-cli FLUSHALL
# run syncd: syncd -SUu -z redis_sync -p vsprofile.ini

#from pysairedis import *
import pysairedis

def switch_shutdown_request_notification(switch_id):
    print " - switch shutdown request"
    print " * swid: " + hex(switch_id)

def switch_state_change_notification(switch_id, switch_oper_status):
    print " - switch state change"
    print " * swid: " + hex(switch_id)
    print " * oper_status: " + str(switch_oper_status)

def port_state_change_notification(count, port_oper_status):
    print " - port state change"
    print " * count: " + str(count)

    for n in range(0,count):
        item = pysairedis.sai_port_oper_status_notification_t_arr_getitem(port_oper_status, n)
        print " * port_id: " + hex(item.port_id)
        print " * port_state: " + str(item.port_state)

def fdb_event_notification(count, data):
    print " - fdb event"
    print " * count: " + str(count)

    for n in range(0,count):
        item = pysairedis.sai_fdb_event_notification_data_t_arr_getitem(data, n)
        print " * event_type: " + str(item.event_type)
        print " * fdb_entry" + str(item.fdb_entry)
        print " * attr_count: " + str(item.attr_count)

profileMap = dict()

profileMap["SAI_WARM_BOOT_READ_FILE"] = "./sai_warmboot.bin"
profileMap["SAI_WARM_BOOT_WRITE_FILE"] = "./sai_warmboot.bin"

status = pysairedis.sai_api_initialize(0,profileMap)
print "initialize: " + str(status)

switch_api = pysairedis.sai_switch_api_t()

status = pysairedis.sai_get_switch_api(switch_api)
print "sai_get_switch_api: " + str(status)

attr = pysairedis.sai_attribute_t();

attr.id = pysairedis.SAI_REDIS_SWITCH_ATTR_RECORD
attr.value.booldata = True

status = switch_api.set_switch_attribute(0, attr)
print "set record mode: " + str(status)

attr.id = pysairedis.SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT
attr.value.u64 = 3000

status = switch_api.set_switch_attribute(0, attr)
print "timeout: " + str(status)

attr.id = pysairedis.SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE
attr.value.s32 = pysairedis.SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC

status = switch_api.set_switch_attribute(0, attr)
print "set communication mode: " + str(status)

attr.id = pysairedis.SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD
attr.value.s32 = pysairedis.SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW

status = switch_api.set_switch_attribute(0, attr)
print "init view: " + str(status)

poid = pysairedis.new_sai_object_id_t_p()

attrs = pysairedis.new_sai_attribute_t_arr(6)

attr.id = pysairedis.SAI_SWITCH_ATTR_INIT_SWITCH
attr.value.booldata = True
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

attr.id = pysairedis.SAI_SWITCH_ATTR_SRC_MAC_ADDRESS
# TODO
#attr.value.mac = "90:B1:1C:F4:A8:53"
pysairedis.sai_attribute_t_arr_setitem(attrs, 1, attr)

# set notification callbacks

attr.id = pysairedis.SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY
attr.value.ptr = pysairedis.sai_get_notification_pointer(pysairedis.SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY, switch_state_change_notification)
pysairedis.sai_attribute_t_arr_setitem(attrs, 2, attr)

attr.id = pysairedis.SAI_SWITCH_ATTR_SWITCH_SHUTDOWN_REQUEST_NOTIFY
attr.value.ptr = pysairedis.sai_get_notification_pointer(pysairedis.SAI_SWITCH_ATTR_SWITCH_SHUTDOWN_REQUEST_NOTIFY, switch_shutdown_request_notification)
pysairedis.sai_attribute_t_arr_setitem(attrs, 3, attr)

attr.id = pysairedis.SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY
attr.value.ptr = pysairedis.sai_get_notification_pointer(pysairedis.SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY, fdb_event_notification)
pysairedis.sai_attribute_t_arr_setitem(attrs, 4, attr)

attr.id = pysairedis.SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY
attr.value.ptr = pysairedis.sai_get_notification_pointer(pysairedis.SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY, port_state_change_notification)
pysairedis.sai_attribute_t_arr_setitem(attrs, 5, attr)

status = switch_api.create_switch(poid, 6, attrs)
print "create_switch: " + str(status) 

swid = pysairedis.sai_object_id_t_p_value(poid)

print "swid: " + hex(swid)

pysairedis.delete_sai_attribute_t_arr(attrs)

attr.id = pysairedis.SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD
attr.value.s32 = pysairedis.SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW

status = switch_api.set_switch_attribute(swid, attr)
print "apply view: " + str(status)

attr.id = pysairedis.SAI_SWITCH_ATTR_PORT_LIST
attr.value.objlist.count = 128
attr.value.objlist.list = pysairedis.new_sai_object_id_t_arr(128)

status = switch_api.get_switch_attribute(swid, 1, attr);
print "get port list: " + str(status)
print "ports count: " + str(attr.value.objlist.count)

portlist = []

for n in range(0, attr.value.objlist.count):
    port = pysairedis.sai_object_id_t_arr_getitem(attr.value.objlist.list, n)
    portlist.append(port)

pysairedis.delete_sai_object_id_t_arr(attr.value.objlist.list)

print "portlist: " + str(portlist)

print "portlist[0]: " + hex(portlist[0])

attr.id = pysairedis.SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID

status = switch_api.get_switch_attribute(swid, 1, attr);
print "get default virtual router: " + str(status)

vrid = attr.value.oid

print "vrid: " + hex(vrid)

lag_api = pysairedis.sai_lag_api_t()

status = pysairedis.sai_get_lag_api(lag_api)
print "sai_get_lag_api: " + str(status)

poid = pysairedis.new_sai_object_id_t_p()
#attrs = pysairedis.new_sai_attribute_t_arr(2)

status = lag_api.create_lag(poid, swid, 0, None)
print "create lag: " + str(status)

lagid = pysairedis.sai_object_id_t_p_value(poid)

pysairedis.delete_sai_object_id_t_p(poid)

print "lagid: " + hex(lagid)

router_interface_api = pysairedis.sai_router_interface_api_t()

status = pysairedis.sai_get_router_interface_api(router_interface_api)
print "sai_get_router_interface_api: " + str(status)

poid = pysairedis.new_sai_object_id_t_p()
attrs = pysairedis.new_sai_attribute_t_arr(3)

attr.id = pysairedis.SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID
attr.value.oid = vrid
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

attr.id = pysairedis.SAI_ROUTER_INTERFACE_ATTR_TYPE
attr.value.s32 = pysairedis.SAI_ROUTER_INTERFACE_TYPE_PORT
pysairedis.sai_attribute_t_arr_setitem(attrs, 1, attr)

attr.id = pysairedis.SAI_ROUTER_INTERFACE_ATTR_PORT_ID
attr.value.oid = portlist[0]
pysairedis.sai_attribute_t_arr_setitem(attrs, 2, attr)

status = router_interface_api.create_router_interface(poid, swid, 3, attrs)
print "create router interface: " + str(status)

rifid = pysairedis.sai_object_id_t_p_value(poid)

pysairedis.delete_sai_object_id_t_p(poid)
pysairedis.delete_sai_attribute_t_arr(attrs)

print "rifid: " + hex(rifid)

next_hop_api = pysairedis.sai_next_hop_api_t()

status = pysairedis.sai_get_next_hop_api(next_hop_api)
print "sai_get_next_hop_api: " + str(status)

poid = pysairedis.new_sai_object_id_t_p()
attrs = pysairedis.new_sai_attribute_t_arr(3)

attr.id = pysairedis.SAI_NEXT_HOP_ATTR_TYPE
attr.value.s32 = pysairedis.SAI_NEXT_HOP_TYPE_IP
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

attr.id = pysairedis.SAI_NEXT_HOP_ATTR_IP
attr.value.ipaddr = pysairedis.sai_ip_address_t_from_string("10.0.0.1")
pysairedis.sai_attribute_t_arr_setitem(attrs, 1, attr)

attr.id = pysairedis.SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID
attr.value.oid = rifid
pysairedis.sai_attribute_t_arr_setitem(attrs, 2, attr)

status = next_hop_api.create_next_hop(poid, swid, 3, attrs)
print "create next hop: " + str(status)

nexthopid = pysairedis.sai_object_id_t_p_value(poid)

pysairedis.delete_sai_object_id_t_p(poid)
pysairedis.delete_sai_attribute_t_arr(attrs)

print "nexthopid: " + hex(nexthopid)

route_api = pysairedis.sai_route_api_t()

status = pysairedis.sai_get_route_api(route_api)
print "sai_get_route_api: " + str(status)

attrs = pysairedis.new_sai_attribute_t_arr(1)

attr.id = pysairedis.SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION
attr.value.s32 = pysairedis.SAI_PACKET_ACTION_DROP
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

re = pysairedis.sai_route_entry_t()

re.destination = pysairedis.sai_ip_prefix_t_from_string("0.0.0.0/0")
re.switch_id = swid
re.vr_id = vrid

status = route_api.create_route_entry(re, 1, attrs)
print "create default route entry: " + str(status)

pysairedis.delete_sai_attribute_t_arr(attrs)

attrs = pysairedis.new_sai_attribute_t_arr(1)

attr.id = pysairedis.SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID
attr.value.oid = nexthopid
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

re = pysairedis.sai_route_entry_t()

re.destination = pysairedis.sai_ip_prefix_t_from_string("100.1.0.1/32")
re.switch_id = swid
re.vr_id = vrid

status = route_api.create_route_entry(re, 1, attrs)
print "create route entry: " + str(status)

pysairedis.delete_sai_attribute_t_arr(attrs)

vlan_api = pysairedis.sai_vlan_api_t()

status = pysairedis.sai_get_vlan_api(vlan_api)
print "sai_get_vlan_api: " + str(status)

poid = pysairedis.new_sai_object_id_t_p()
attrs = pysairedis.new_sai_attribute_t_arr(1)

attr.id = pysairedis.SAI_VLAN_ATTR_VLAN_ID
attr.value.u16 = 11
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

status = vlan_api.create_vlan(poid, swid, 1, attrs)
print "create vlan: " + str(status)

vlanid = pysairedis.sai_object_id_t_p_value(poid)

pysairedis.delete_sai_object_id_t_p(poid)
pysairedis.delete_sai_attribute_t_arr(attrs)

print "vlanid: " + hex(vlanid)

fdb_api = pysairedis.sai_fdb_api_t()

status = pysairedis.sai_get_fdb_api(fdb_api)
print "sai_get_fdb_api: " + str(status)

attrs = pysairedis.new_sai_attribute_t_arr(2)

attr.id = pysairedis.SAI_FDB_ENTRY_ATTR_TYPE
attr.value.s32 = pysairedis.SAI_FDB_ENTRY_TYPE_DYNAMIC
pysairedis.sai_attribute_t_arr_setitem(attrs, 0, attr)

attr.id = pysairedis.SAI_FDB_ENTRY_ATTR_PACKET_ACTION
attr.value.s32 = pysairedis.SAI_PACKET_ACTION_FORWARD
pysairedis.sai_attribute_t_arr_setitem(attrs, 1, attr)

fe = pysairedis.sai_fdb_entry_t()

# TODO
#fe.mac_address = pysairedis.sai_mac_t_from_string("FE:54:00:B3:06:8C")
fe.switch_id = swid
fe.bv_id = vlanid

status = fdb_api.create_fdb_entry(fe, 2, attrs)
print "create default fdb entry: " + str(status)

pysairedis.delete_sai_attribute_t_arr(attrs)

status = fdb_api.remove_fdb_entry(fe)
print "remove fdb entry: " + str(status)

attr.id = pysairedis.SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES
attr.value.u16 = 32

status = vlan_api.set_vlan_attribute(vlanid, attr)
print "set vlan attribute: " + str(status)

r = vlan_api.remove_vlan(vlanid)
print "remove vlan: " + str(status)

attr.id = pysairedis.SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION
attr.value.s32 = pysairedis.SAI_PACKET_ACTION_FORWARD

status = route_api.set_route_entry_attribute(re, attr)
print "set route entry: " + str(status)

attr.id = pysairedis.SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION

status = route_api.get_route_entry_attribute(re, 1, attr)
print "get route entry packet action: " + str(status)

print "packet action: " + str(attr.value.s32)

status = route_api.remove_route_entry(re)
print "remove route entry: " + str(status)

status = pysairedis.sai_api_uninitialize()
print "uninitialize: " + str(status)
