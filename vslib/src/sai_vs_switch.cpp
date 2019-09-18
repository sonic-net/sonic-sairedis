#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"

#include "swss/selectableevent.h"
#include "swss/netmsg.h"
#include "swss/netlink.h"
#include "swss/select.h"
#include "swss/netdispatcher.h"

#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <linux/if.h>

#include <thread>

/**
 * @brief Get SwitchState by switch id.
 *
 * Function will get shared object for switch state.  This function is thread
 * safe and it's only intended to use inside threads.
 *
 * @param switch_id Switch ID
 *
 * @return SwitchState object or null ptr if not found.
 */
std::shared_ptr<SwitchState> vs_get_switch_state(
        _In_ sai_object_id_t switch_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    auto it = g_switch_state_map.find(switch_id);

    if (it == g_switch_state_map.end())
    {
        return nullptr;
    }

    return it->second;
}

void update_port_oper_status(
        _In_ sai_object_id_t port_id,
        _In_ sai_port_oper_status_t port_oper_status)
{
    MUTEX();

    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;
    attr.value.s32 = port_oper_status;

    sai_status_t status = vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to update port status %s: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_port_oper_status(port_oper_status).c_str());
    }
}

class LinkMsg : public swss::NetMsg
{
    public:

        LinkMsg(sai_object_id_t switch_id)
            : m_switch_id(switch_id)
        {
            SWSS_LOG_ENTER();
        }

        virtual void onMsg(int nlmsg_type, struct nl_object *obj)
        {
            SWSS_LOG_ENTER();

            if (nlmsg_type != RTM_NEWLINK)
            {
                SWSS_LOG_WARN("unsupported nlmsg_type: %d", nlmsg_type);
                return;
            }

            // new link

            struct rtnl_link *link = (struct rtnl_link *)obj;

            int             if_index = rtnl_link_get_ifindex(link);
            unsigned int    if_flags = rtnl_link_get_flags(link); // IFF_LOWER_UP and IFF_RUNNING
            const char*     if_name  = rtnl_link_get_name(link);

            if (strncmp(if_name, SAI_VS_VETH_PREFIX, sizeof(SAI_VS_VETH_PREFIX) - 1) != 0 &&
                    g_ifname_to_lanes.find(if_name) == g_ifname_to_lanes.end())
            {
                SWSS_LOG_INFO("skipping newlink for %s", if_name);
                return;
            }

            SWSS_LOG_NOTICE("newlink: ifindex: %d, ifflags: 0x%x, ifname: %s",
                    if_index,
                    if_flags,
                    if_name);

            std::string ifname(if_name);

            std::shared_ptr<SwitchState> sw = vs_get_switch_state(m_switch_id);

            if (sw == nullptr)
            {
                SWSS_LOG_ERROR("failed to get switch state for switch id %s",
                        sai_serialize_object_id(m_switch_id).c_str());
                return;
            }

            auto port_id = sw->getPortIdFromIfName(ifname);

            if (port_id == SAI_NULL_OBJECT_ID)
            {
                SWSS_LOG_ERROR("failed to find port id for interface %s", ifname.c_str());
                return;
            }

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;

            if (vs_switch_api.get_switch_attribute(m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for switch %s",
                        sai_serialize_object_id(m_switch_id).c_str());
                return;
            }

            sai_port_state_change_notification_fn callback =
                (sai_port_state_change_notification_fn)attr.value.ptr;

            if (callback == NULL)
            {
                return;
            }

            sai_port_oper_status_notification_t data;

            data.port_id = port_id;
            data.port_state = (if_flags & IFF_LOWER_UP) ? SAI_PORT_OPER_STATUS_UP : SAI_PORT_OPER_STATUS_DOWN;

            attr.id = SAI_PORT_ATTR_OPER_STATUS;

            if (vs_port_api.get_port_attribute(port_id, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to get port attribute SAI_PORT_ATTR_OPER_STATUS");
            }
            else
            {
                if ((sai_port_oper_status_t)attr.value.s32 == data.port_state)
                {
                    SWSS_LOG_DEBUG("port oper status didn't changed, will not send notification");
                    return;
                }
            }

            update_port_oper_status(port_id, data.port_state);

            SWSS_LOG_DEBUG("executing callback SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for port %s: %s",
                    sai_serialize_object_id(data.port_id).c_str(),
                    sai_serialize_port_oper_status(data.port_state).c_str());

            callback(1, &data);
        }

    private:

        sai_object_id_t m_switch_id;
};

void linkmsg_thread(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<SwitchState> sw = vs_get_switch_state(switch_id);

    if (sw == nullptr)
    {
        SWSS_LOG_ERROR("failed to get switch state for switch id %s",
                sai_serialize_object_id(switch_id).c_str());
        return;
    }

    LinkMsg linkMsg(switch_id);

    swss::NetDispatcher::getInstance().registerMessageHandler(RTM_NEWLINK, &linkMsg);
    swss::NetDispatcher::getInstance().registerMessageHandler(RTM_DELLINK, &linkMsg);

    SWSS_LOG_NOTICE("netlink msg listener started for switch %s",
            sai_serialize_object_id(switch_id).c_str());

    while (sw->getRunLinkThread())
    {
        try
        {
            swss::NetLink netlink;

            swss::Select s;

            netlink.registerGroup(RTNLGRP_LINK);
            netlink.dumpRequest(RTM_GETLINK);

            s.addSelectable(&netlink);
            s.addSelectable(sw->getLinkThreadEvent());

            while (sw->getRunLinkThread())
            {
                swss::Selectable *temps = NULL;

                int result = s.select(&temps);

                SWSS_LOG_INFO("select ended: %d", result);
            }
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("exception: %s", e.what());
            return;
        }
    }
}

void vs_create_netlink_message_listener(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == false)
    {
        return;
    }

    auto sw = vs_get_switch_state(switch_id);

    if (sw == nullptr)
    {
        SWSS_LOG_ERROR("failed to get switch state for switch id %s",
                sai_serialize_object_id(switch_id).c_str());
        return;
    }

    sw->setRunLinkThread(true);

    std::shared_ptr<std::thread> linkThread =
        std::make_shared<std::thread>(linkmsg_thread, switch_id);

    sw->setLinkThread(linkThread);
}

void vs_remove_netlink_message_listener(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == false)
    {
        return;
    }

    auto sw = vs_get_switch_state(switch_id);

    if (sw == nullptr)
    {
        SWSS_LOG_ERROR("failed to get switch state for switch id %s",
                sai_serialize_object_id(switch_id).c_str());
        return;
    }

    sw->setRunLinkThread(false);

    sw->getLinkThreadEvent()->notify();

    sw->getLinkThread()->join();

    SWSS_LOG_NOTICE("removed netlink thread listener");
}

sai_status_t vs_create_switch(
        _Out_ sai_object_id_t *switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_create_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            SAI_NULL_OBJECT_ID, // no switch id since we create switch
            attr_count,
            attr_list,
            &vs_generic_create);

    if (status == SAI_STATUS_SUCCESS)
    {
        vs_create_netlink_message_listener(*switch_id);
    }

    return status;
}

sai_status_t vs_remove_switch(
            _In_ sai_object_id_t switch_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    vs_remove_netlink_message_listener(switch_id);

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            &vs_generic_remove);
}

VS_SET(SWITCH,switch);
VS_GET(SWITCH,switch);
VS_GENERIC_STATS(SWITCH, switch);

const sai_switch_api_t vs_switch_api = {

    vs_create_switch,
    vs_remove_switch,
    vs_set_switch_attribute,
    vs_get_switch_attribute,
    VS_GENERIC_STATS_API(switch)
};
