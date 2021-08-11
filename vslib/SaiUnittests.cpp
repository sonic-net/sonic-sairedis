#include "Sai.h"
#include "SaiInternal.h"

#include "saivs.h"
#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/select.h"

using namespace saivs;

void Sai::startUnittestThread()
{
    SWSS_LOG_ENTER();

    m_unittestChannelThreadEvent = std::make_shared<swss::SelectableEvent>();

    // TODO may require to pass correct DB information
    m_dbNtf = std::make_shared<swss::DBConnector>("ASIC_DB", 0);

    m_unittestChannelNotificationConsumer = std::make_shared<swss::NotificationConsumer>(m_dbNtf.get(), SAI_VS_UNITTEST_CHANNEL);

    m_unittestChannelRun = true;

    m_unittestChannelThread = std::make_shared<std::thread>(std::thread(&Sai::unittestChannelThreadProc, this));
}

void Sai::stopUnittestThread()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    if (m_unittestChannelRun)
    {
        m_unittestChannelRun = false;

        // notify thread that it should end
        m_unittestChannelThreadEvent->notify();

        m_unittestChannelThread->join();
    }

    SWSS_LOG_NOTICE("end");
}

void Sai::channelOpEnableUnittest(
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    bool enable = (key == "true");

    m_meta->meta_unittests_enable(enable);
}

void Sai::channelOpSetReadOnlyAttribute(
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    for (const auto &v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    if (values.size() != 1)
    {
        SWSS_LOG_ERROR("expected 1 value only, but given: %zu", values.size());
        return;
    }

    const std::string &str_object_type = key.substr(0, key.find(":"));
    const std::string &str_object_id = key.substr(key.find(":") + 1);

    sai_object_type_t object_type;
    sai_deserialize_object_type(str_object_type, object_type);

    if (object_type == SAI_OBJECT_TYPE_NULL || object_type >= SAI_OBJECT_TYPE_EXTENSIONS_MAX)
    {
        SWSS_LOG_ERROR("invalid object type: %d", object_type);
        return;
    }

    auto info = sai_metadata_get_object_type_info(object_type);

    if (info->isnonobjectid)
    {
        SWSS_LOG_ERROR("non object id %s is not supported yet", str_object_type.c_str());
        return;
    }

    sai_object_id_t object_id;

    sai_deserialize_object_id(str_object_id, object_id);

    sai_object_type_t ot = objectTypeQuery(object_id);

    if (ot != object_type)
    {
        SWSS_LOG_ERROR("object type is different than provided %s, but oid is %s",
                str_object_type.c_str(), sai_serialize_object_type(ot).c_str());
        return;
    }

    sai_object_id_t switch_id = switchIdQuery(object_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to find switch id for oid %s", str_object_id.c_str());
        return;
    }

    // oid is validated and we got switch id

    const std::string &str_attr_id = fvField(values.at(0));
    const std::string &str_attr_value = fvValue(values.at(0));

    auto meta = sai_metadata_get_attr_metadata_by_attr_id_name(str_attr_id.c_str());

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("failed to find attr %s", str_attr_id.c_str());
        return;
    }

    if (meta->objecttype != ot)
    {
        SWSS_LOG_ERROR("attr %s belongs to different object type than oid: %s",
                str_attr_id.c_str(), sai_serialize_object_type(ot).c_str());
        return;
    }

    // we got attr metadata

    sai_attribute_t attr;

    attr.id = meta->attrid;

    sai_deserialize_attr_value(str_attr_value, *meta, attr);

    SWSS_LOG_NOTICE("switch id is %s", sai_serialize_object_id(switch_id).c_str());

    sai_status_t status = m_meta->meta_unittests_allow_readonly_set_once(meta->objecttype, meta->attrid);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to enable SET readonly attribute once: %s", sai_serialize_status(status).c_str());
        return;
    }

    sai_object_meta_key_t meta_key = { .objecttype = ot, .objectkey = { .key = { .object_id = object_id } } };

    status = m_meta->set(meta_key, &attr); // name hidden in base class (but same name overloaded in derived class)

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to set %s to %s on %s",
                str_attr_id.c_str(), str_attr_value.c_str(), str_object_id.c_str());
    }
    else
    {
        SWSS_LOG_NOTICE("SUCCESS to set %s to %s on %s",
                str_attr_id.c_str(), str_attr_value.c_str(), str_object_id.c_str());
    }

    sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
}

void Sai::channelOpSetStats(
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    // NOTE: add support for non object id stats

    sai_object_id_t oid;

    sai_deserialize_object_id(key, oid);

    sai_object_type_t ot = objectTypeQuery(oid);

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("invalid object id: %s", key.c_str());
        return;
    }

    sai_object_id_t switch_id = switchIdQuery(oid);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("unable to get switch_id from oid: %s", key.c_str());
        return;
    }

    auto statenum = sai_metadata_get_object_type_info(ot)->statenum;

    if (statenum == NULL)
    {
        SWSS_LOG_ERROR("object %s does not support statistics",
                sai_serialize_object_type(ot).c_str());
        return;
    }

    /*
     * Check if object for statistics have statistic map created, if not
     * create empty map.
     */

    std::map<sai_stat_id_t, uint64_t> stats;

    for (auto v: values)
    {
        // value format: stat_enum_name:uint64

        auto name = fvField(v);
        auto val  = fvValue(v);

        uint64_t value;

        if (sscanf(val.c_str(), "%" PRIu64, &value) != 1)
        {
            SWSS_LOG_ERROR("failed to deserialize %s as counter value uint64_t", val.c_str());
            continue;
        }

        // linear search

        int enumvalue = -1;

        for (size_t i = 0; i < statenum->valuescount; ++i)
        {
            if (statenum->valuesnames[i] == name)
            {
                enumvalue = statenum->values[i];
                break;
            }
        }

        if (enumvalue == -1)
        {
            SWSS_LOG_ERROR("failed to find enum value: %s", name.c_str());
            continue;
        }

        SWSS_LOG_INFO("writing %s = %lu on %s", name.c_str(), value, key.c_str());

        stats[enumvalue] = value;
    }

    m_vsSai->debugSetStats(oid, stats);
}

void Sai::handleUnittestChannelOp(
        _In_ const std::string &op,
        _In_ const std::string &key,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    MUTEX();

    SWSS_LOG_ENTER();

    /*
     * Since we will access and modify DB we need to be under mutex.
     *
     * NOTE: since this unittest channel is handled in thread, then that means
     * there is a DELAY from producer and consumer thread in VS, so if user
     * will set value on the specific READ_ONLY value he should wait for some
     * time until that value will be propagated to virtual switch.
     */

    SWSS_LOG_NOTICE("op = %s, key = %s", op.c_str(), key.c_str());

    for (const auto &v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    if (op == SAI_VS_UNITTEST_ENABLE_UNITTESTS)
    {
        channelOpEnableUnittest(key, values);
    }
    else if (op == SAI_VS_UNITTEST_SET_RO_OP)
    {
        channelOpSetReadOnlyAttribute(key, values);
    }
    else if (op == SAI_VS_UNITTEST_SET_STATS_OP)
    {
        channelOpSetStats(key, values);
    }
    else
    {
        SWSS_LOG_ERROR("unknown unittest operation: %s", op.c_str());
    }
}

void Sai::unittestChannelThreadProc()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("enter VS unittest channel thread");

    swss::Select s;

    s.addSelectable(m_unittestChannelNotificationConsumer.get());
    s.addSelectable(m_unittestChannelThreadEvent.get());

    while (m_unittestChannelRun)
    {
        swss::Selectable *sel = nullptr;

        int result = s.select(&sel);

        if (sel == m_unittestChannelThreadEvent.get())
        {
            // user requested shutdown_switch
            break;
        }

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            std::string op;
            std::string data;
            std::vector<swss::FieldValueTuple> values;

            m_unittestChannelNotificationConsumer->pop(op, data, values);

            SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

            try
            {
                handleUnittestChannelOp(op, data, values);
            }
            catch (const std::exception &e)
            {
                SWSS_LOG_ERROR("Exception: op = %s, data = %s, %s", op.c_str(), data.c_str(), e.what());
            }
        }
    }

    SWSS_LOG_NOTICE("exit VS unittest channel thread");
}
