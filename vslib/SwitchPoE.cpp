#include "SwitchPoE.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace saivs;

SwitchPoE::SwitchPoE(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchPoE::SwitchPoE(
    _In_ sai_object_id_t switch_id,
    _In_ std::shared_ptr<RealObjectIdManager> manager,
    _In_ std::shared_ptr<SwitchConfig> config,
    _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchPoE::~SwitchPoE()
{
    SWSS_LOG_ENTER();

    // empty
}

sai_status_t SwitchPoE::set_switch_default_attributes()
{
    SWSS_LOG_ENTER();

    sai_status_t ret;

    // Fill this with supported SAI_OBJECT_TYPEs
    int32_t supported_obj_list[] = {
        SAI_OBJECT_TYPE_NULL,
        SAI_OBJECT_TYPE_POE_DEVICE,
        SAI_OBJECT_TYPE_POE_PSE,
        SAI_OBJECT_TYPE_POE_PORT,
    };
    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_POE_DEVICE_LIST;
    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_WARM_RECOVER;
    attr.value.booldata = false;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.s32 = SAI_SWITCH_TYPE_POE;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    // v0.1
    attr.id = SAI_SWITCH_ATTR_FIRMWARE_MAJOR_VERSION;
    attr.value.u32 = 0;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_FIRMWARE_MINOR_VERSION;
    attr.value.u32 = 1;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_SUPPORTED_OBJECT_TYPE_LIST;
    attr.value.s32list.count = sizeof(supported_obj_list) / sizeof(int32_t);
    attr.value.s32list.list = supported_obj_list;
    ret = set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);

    return ret;
}

sai_status_t SwitchPoE::initialize_default_objects(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_switch_default_attributes());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchPoE::create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_POE_DEVICE)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createPoeDevice(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_POE_PORT)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createPoePort(object_id, switch_id, attr_count, attr_list);
    }

    return create_internal(object_type, serializedObjectId, switch_id, attr_count, attr_list);
}

sai_status_t SwitchPoE::createPoeDevice(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    auto power_limit = sai_metadata_get_attr_by_id(SAI_POE_DEVICE_ATTR_POWER_LIMIT_MODE, attr_count, attr_list);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_POE_DEVICE, sid, switch_id, attr_count, attr_list));

    sai_attribute_t attr;

    /* if not set in create_internal then use default values*/
    if (!power_limit)
    {
        attr.id = SAI_POE_DEVICE_ATTR_POWER_LIMIT_MODE;
        attr.value.u32 = SAI_POE_DEVICE_LIMIT_MODE_CLASS;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_POE_DEVICE, object_id, &attr));
    }

    /* update the list of all POE devices on switch */
    m_poe_device_list.push_back(object_id);

    auto device_count = (uint32_t)m_poe_device_list.size();
    attr.id = SAI_SWITCH_ATTR_POE_DEVICE_LIST;
    attr.value.objlist.count = device_count;
    attr.value.objlist.list = m_poe_device_list.data();
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchPoE::createPoePort(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    auto power_limit = sai_metadata_get_attr_by_id(SAI_POE_PORT_ATTR_POWER_LIMIT, attr_count, attr_list);
    auto power_priority = sai_metadata_get_attr_by_id(SAI_POE_PORT_ATTR_POWER_PRIORITY, attr_count, attr_list);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_POE_PORT, sid, switch_id, attr_count, attr_list));

    sai_attribute_t attr;

    /* default admin state is down as defined in SAI */
    attr.id = SAI_POE_PORT_ATTR_ADMIN_ENABLED_STATE;
    attr.value.booldata = false;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_POE_PORT, object_id, &attr));

    /* if not set in create_internal then use default values*/
    if (!power_limit)
    {
        attr.id = SAI_POE_PORT_ATTR_POWER_LIMIT;
        attr.value.u32 = 0;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_POE_PORT, object_id, &attr));
    }
    if (!power_priority)
    {
        attr.id = SAI_POE_PORT_ATTR_POWER_PRIORITY;
        attr.value.u32 = SAI_POE_PORT_POWER_PRIORITY_TYPE_HIGH;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_POE_PORT, object_id, &attr));
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchPoE::refresh_read_only(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = meta->attrid;
    if (meta->objecttype == SAI_OBJECT_TYPE_POE_DEVICE)
    {
        switch (meta->attrid)
        {
        case SAI_POE_DEVICE_ATTR_HARDWARE_INFO:
            strncpy(attr.value.chardata, "hardware info", sizeof(attr.value.chardata));
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_DEVICE_ATTR_POE_PSE_LIST:
            return refresh_poe_pse_list(meta, object_id);
        case SAI_POE_DEVICE_ATTR_POE_PORT_LIST:
            return refresh_poe_port_list(meta, object_id);
        case SAI_POE_DEVICE_ATTR_TOTAL_POWER:
            attr.value.u32 = 100;
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_DEVICE_ATTR_POWER_CONSUMPTION:
            attr.value.u32 = 10000;
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_DEVICE_ATTR_VERSION:
            strncpy(attr.value.chardata, "version", sizeof(attr.value.chardata));
            return set(meta->objecttype, object_id, &attr);
        }

    }
    if (meta->objecttype == SAI_OBJECT_TYPE_POE_PSE)
    {
        switch (meta->attrid)
        {
        case SAI_POE_PSE_ATTR_TEMPERATURE:
            attr.value.u32 = 25;
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PSE_ATTR_STATUS:
            attr.value.u32 = SAI_POE_PSE_STATUS_TYPE_ACTIVE;
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PSE_ATTR_SOFTWARE_VERSION:
            strncpy(attr.value.chardata, "software version", sizeof(attr.value.chardata));
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PSE_ATTR_HARDWARE_VERSION:
            strncpy(attr.value.chardata, "hardware version", sizeof(attr.value.chardata));
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PSE_ATTR_ID:
        case SAI_POE_PSE_ATTR_DEVICE_ID:
            return SAI_STATUS_SUCCESS;
        }
    }
    if (meta->objecttype == SAI_OBJECT_TYPE_POE_PORT)
    {
        switch (meta->attrid)
        {
        case SAI_POE_PORT_ATTR_STANDARD:
            attr.value.u32 = SAI_POE_PORT_STANDARD_TYPE_BT_TYPE3;
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PORT_ATTR_CONSUMPTION:
            /* random values */
            attr.value.portpowerconsumption = (sai_poe_port_power_consumption_t){
                .active_channel=SAI_POE_PORT_ACTIVE_CHANNEL_TYPE_A_AND_B,
                .voltage=50000,
                .current=200,
                .consumption=10000,
                .signature_type=SAI_POE_PORT_SIGNATURE_TYPE_DUAL,
                .class_method=SAI_POE_PORT_CLASS_METHOD_TYPE_AUTO_CLASS,
                .measured_class_a=1,
                .assigned_class_a=2,
                .measured_class_b=3,
                .assigned_class_b=4
            };
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PORT_ATTR_STATUS:
            attr.value.u32 = SAI_POE_PORT_STATUS_TYPE_DELIVERING_POWER;
            return set(meta->objecttype, object_id, &attr);
        case SAI_POE_PORT_ATTR_FRONT_PANEL_ID:
        case SAI_POE_PORT_ATTR_DEVICE_ID:
            return SAI_STATUS_SUCCESS;
        }
    }

    auto mmeta = m_meta.lock();

    if (mmeta)
    {
        if (mmeta->meta_unittests_enabled())
        {
            SWSS_LOG_NOTICE("unittests enabled, SET could be performed on %s, not recalculating", meta->attridname);

            return SAI_STATUS_SUCCESS;
        }
    }
    else
    {
        SWSS_LOG_WARN("meta pointer expired");
    }

    SWSS_LOG_WARN("need to recalculate RO: %s", meta->attridname);

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchPoE::refresh_poe_pse_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t poe_device_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_POE_PSE_ATTR_DEVICE_ID;
    attr.value.oid = poe_device_id;

    findObjects(SAI_OBJECT_TYPE_POE_PSE, attr, m_poe_pse_list);
    std::sort(m_poe_pse_list.begin(), m_poe_pse_list.end());

    auto pse_count = (uint32_t)m_poe_pse_list.size();

    attr.id = SAI_POE_DEVICE_ATTR_POE_PSE_LIST;
    attr.value.objlist.count = pse_count;
    attr.value.objlist.list = m_poe_pse_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_POE_DEVICE, poe_device_id, &attr));

    SWSS_LOG_NOTICE("refreshed POE PSE list, current PSE number: %u", pse_count);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchPoE::refresh_poe_port_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t poe_device_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_POE_PORT_ATTR_DEVICE_ID;
    attr.value.oid = poe_device_id;

    findObjects(SAI_OBJECT_TYPE_POE_PORT, attr, m_poe_port_list);
    std::sort(m_poe_port_list.begin(), m_poe_port_list.end());

    auto port_count = (uint32_t)m_poe_port_list.size();

    attr.id = SAI_POE_DEVICE_ATTR_POE_PORT_LIST;
    attr.value.objlist.count = port_count;
    attr.value.objlist.list = m_poe_port_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_POE_DEVICE, poe_device_id, &attr));

    SWSS_LOG_NOTICE("refreshed POE port list, current port number: %u", port_count);
    return SAI_STATUS_SUCCESS;
}
