#include "SwitchBCM81724.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace saivs;

SwitchBCM81724::SwitchBCM81724(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchBCM81724::SwitchBCM81724(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchBCM81724::~SwitchBCM81724()
{
    SWSS_LOG_ENTER();

    // empty
}

sai_status_t SwitchBCM81724::create_port_dependencies(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_WARN("check attributes and set, FIXME");

    // this method is post create action on generic create object

    sai_attribute_t attr;

    // default admin state is down as defined in SAI

    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    attr.value.booldata = false;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_OPER_STATUS;
    attr.value.s32 = SAI_PORT_OPER_STATUS_DOWN;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_PORT_SERDES_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchBCM81724::initialize_default_objects(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_switch_default_attributes());
    CHECK_STATUS(create_default_trap_group());
    CHECK_STATUS(set_acl_entry_min_prio());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchBCM81724::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    return SwitchStateBase::set(objectType, objectId, attr);
}

sai_status_t SwitchBCM81724::create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // Bypass MACsec creating because the existing implementation of MACsec cannot be directly used by Gearbox
    if (is_macsec_type(object_type))
    {
        SWSS_LOG_INFO("Bypass creating %s", sai_serialize_object_type(object_type).c_str());

        return create_internal(object_type, serializedObjectId, switch_id, attr_count, attr_list);
    }

    return SwitchStateBase::create(object_type, serializedObjectId, switch_id, attr_count, attr_list);
}

sai_status_t SwitchBCM81724::remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    // Bypass MACsec removing because the existing implementation of MACsec cannot be directly used by Gearbox
    if (is_macsec_type(object_type))
    {
        SWSS_LOG_INFO("Bypass removing %s", sai_serialize_object_type(object_type).c_str());

        return remove_internal(object_type, serializedObjectId);
    }

    return SwitchStateBase::remove(object_type, serializedObjectId);
}

sai_status_t SwitchBCM81724::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    // Bypass MACsec setting because the existing implementation of MACsec cannot be directly used by Gearbox
    if (is_macsec_type(objectType) ||
        (objectType == SAI_OBJECT_TYPE_ACL_ENTRY && attr && attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW))
    {
        SWSS_LOG_INFO("Bypass setting %s", sai_serialize_object_type(objectType).c_str());

        return set_internal(objectType, serializedObjectId, attr);;
    }

    return SwitchStateBase::set(objectType, serializedObjectId, attr);
}

sai_status_t SwitchBCM81724::refresh_port_list(
        _In_ const sai_attr_metadata_t *meta)
{
    SWSS_LOG_ENTER();

    // since now port can be added or removed, we need to update port list
    // dynamically

    sai_attribute_t attr;

    m_port_list.clear();

    // iterate via ASIC state to find all the ports

    for (const auto& it: m_objectHash.at(SAI_OBJECT_TYPE_PORT))
    {
        sai_object_id_t port_id;
        sai_deserialize_object_id(it.first, port_id);

        m_port_list.push_back(port_id);
    }

    uint32_t port_count = (uint32_t)m_port_list.size();

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = port_count;
    attr.value.objlist.list = m_port_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = port_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    SWSS_LOG_NOTICE("refreshed port list, current port number: %zu, not counting cpu port", m_port_list.size());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchBCM81724::set_switch_default_attributes()
{
    SWSS_LOG_ENTER();

    sai_status_t ret;

    // Fill this with supported SAI_OBJECT_TYPEs
    int32_t supported_obj_list[] = {
        SAI_OBJECT_TYPE_NULL,
        SAI_OBJECT_TYPE_PORT
    };
    SWSS_LOG_INFO("create switch default attributes");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_ACTIVE_PORTS;
    attr.value.u32 = 0;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_WARM_RECOVER;
    attr.value.booldata = false;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.s32 = SAI_SWITCH_TYPE_PHY;

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

// override of base class but returning failure in most cases. GB phys implement very little

sai_status_t SwitchBCM81724::refresh_read_only(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (meta->objecttype == SAI_OBJECT_TYPE_SWITCH)
    {
        switch (meta->attrid)
        {
            case SAI_SWITCH_ATTR_NUMBER_OF_ACTIVE_PORTS:
            case SAI_SWITCH_ATTR_PORT_LIST:
                return refresh_port_list(meta); // TODO should implement, override and call on init create_ports

            case SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP:
            case SAI_SWITCH_ATTR_FIRMWARE_MAJOR_VERSION:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY:
                return SAI_STATUS_SUCCESS;
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_PORT)
    {
        switch (meta->attrid)
        {
            //case SAI_PORT_ATTR_SUPPORTED_FEC_MODE:
            //case SAI_PORT_ATTR_SUPPORTED_AUTO_NEG_MODE:
            //case SAI_PORT_ATTR_REMOTE_ADVERTISED_FEC_MODE:
            //case SAI_PORT_ATTR_ADVERTISED_FEC_MODE:
            //    // TODO where is code that is doing refresh for those?
            //    return SAI_STATUS_SUCCESS;

                /*
                 * This status is based on hostif vEthernetX status.
                 */

            case SAI_PORT_ATTR_OPER_STATUS:
            case SAI_PORT_ATTR_PORT_SERDES_ID:
                return SAI_STATUS_SUCCESS;

            case SAI_PORT_ATTR_OPER_SPEED:
                return refresh_port_oper_speed(object_id);
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_DEBUG_COUNTER && meta->attrid == SAI_DEBUG_COUNTER_ATTR_INDEX)
    {
        return SAI_STATUS_SUCCESS;  // XXX not sure for gearbox
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_MACSEC && meta->attrid == SAI_MACSEC_ATTR_SCI_IN_INGRESS_MACSEC_ACL)
    {
        return refresh_macsec_sci_in_ingress_macsec_acl(object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        return refresh_macsec_sa_stat(object_id);
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

sai_status_t SwitchBCM81724::create_default_trap_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default trap group");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchBCM81724::warm_boot_initialize_objects()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("warm boot is not implemented for SwitchBCM81724");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

bool SwitchBCM81724::is_macsec_type(_In_ sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    switch(object_type)
    {
        case SAI_OBJECT_TYPE_MACSEC_PORT:

        case SAI_OBJECT_TYPE_MACSEC_SC:

        case SAI_OBJECT_TYPE_MACSEC_SA:

            return true;

        default: break;
    }

    return false;
}
