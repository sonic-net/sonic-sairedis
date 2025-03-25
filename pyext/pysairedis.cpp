#include "pysairedis.h"

#include "lib/sairedis.h"
#include "swss/logger.h"

#include "meta/sai_serialize.h"

static std::map<std::string, std::string> g_profileMap;
static std::map<std::string, std::string>::iterator g_profileMapIterator = g_profileMap.begin();

static const char *profile_get_value (
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char *variable)
{
    SWSS_LOG_ENTER();

    auto it = g_profileMap.find(variable);

    if (it == g_profileMap.end())
        return NULL;
    return it->second.c_str();
}

static int profile_get_next_value (
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char **variable,
        _Out_ const char **value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        // Restarts enumeration
        g_profileMapIterator = g_profileMap.begin();
    }
    else if (g_profileMapIterator == g_profileMap.end())
    {
        return -1;
    }
    else
    {
        *variable = g_profileMapIterator->first.c_str();
        *value = g_profileMapIterator->second.c_str();
        g_profileMapIterator++;
    }

    if (g_profileMapIterator != g_profileMap.end())
        return 0;

    return -1;
}

static const sai_service_method_table_t g_smt = {
    profile_get_value,
    profile_get_next_value
};

sai_status_t sai_api_initialize(
        _In_ uint64_t flags,
        _In_ const std::map<std::string, std::string>& profileMap)
{
    g_profileMap = profileMap;
    g_profileMapIterator = g_profileMap.begin();

    return sai_api_initialize(flags, &g_smt);
}

sai_mac_t* sai_mac_t_from_string(const std::string& s)
{
    sai_mac_t *mac = (sai_mac_t*)calloc(1, sizeof(sai_mac_t));

    sai_deserialize_mac(s, *mac);

    return mac;
}

sai_ip_address_t* sai_ip_address_t_from_string(const std::string& s)
{
    sai_ip_address_t* ip = (sai_ip_address_t*)calloc(1, sizeof(sai_ip_address_t));

    sai_deserialize_ip_address(s, *ip);
    return ip;
}

sai_ip_prefix_t* sai_ip_prefix_t_from_string(const std::string& s)
{
    sai_ip_prefix_t* ip = (sai_ip_prefix_t*)calloc(1, sizeof(sai_ip_prefix_t));

    sai_deserialize_ip_prefix(s, *ip);

    return ip;
}

// sai notification handling

PyObject *py_convert_sai_fdb_event_notification_data_t_to_PyObject(const sai_fdb_event_notification_data_t*ntf);
PyObject *py_convert_sai_bfd_session_state_notification_t_to_PyObject(const sai_bfd_session_state_notification_t*ntf);
PyObject *py_convert_sai_port_oper_status_notification_t_to_PyObject(const sai_port_oper_status_notification_t*ntf);
PyObject *py_convert_sai_queue_deadlock_notification_data_t_to_PyObject(const sai_queue_deadlock_notification_data_t*ntf);

static PyObject * py_fdb_event_notification = NULL;
static PyObject * py_port_state_change_notification = NULL;
static PyObject * py_queue_pfc_deadlock_notification = NULL;
static PyObject * py_switch_shutdown_request_notification = NULL;
static PyObject * py_switch_state_change_notification = NULL;
static PyObject * py_bfd_session_state_change_notification = NULL;
static PyObject * py_tam_tel_type_config_change_notification = NULL;

void call_python(PyObject* callObject, PyObject* arglist)
{
    PyObject* result = PyObject_CallObject(callObject, arglist);

    if (result == NULL)
    {
        PyObject* pPyErr = PyErr_Occurred();

        if (pPyErr)
        {
            PyErr_Print();
            return;
        }

        SWSS_LOG_THROW("callback call failed");
    }

    Py_DECREF(result);
}

static void sai_fdb_event_notification(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    PyObject* obj = py_convert_sai_fdb_event_notification_data_t_to_PyObject(data);

    PyObject* arglist = Py_BuildValue("(iO)", count, obj);

    call_python(py_fdb_event_notification, arglist);

    Py_DECREF(obj);

    Py_DECREF(arglist);
}

static void sai_port_state_change_notification(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    PyObject* obj = py_convert_sai_port_oper_status_notification_t_to_PyObject(data);

    PyObject* arglist = Py_BuildValue("(iO)", count, obj);

    call_python(py_port_state_change_notification, arglist);

    Py_DECREF(obj);

    Py_DECREF(arglist);
}

static void sai_queue_pfc_deadlock_notification(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    PyObject* obj = py_convert_sai_queue_deadlock_notification_data_t_to_PyObject(data);

    PyObject* arglist = Py_BuildValue("(iO)", count, obj);

    call_python(py_queue_pfc_deadlock_notification, arglist);

    Py_DECREF(obj);

    Py_DECREF(arglist);
}

static void sai_switch_shutdown_request_notification(
        _In_ sai_object_id_t switch_id)
{
    PyObject* arglist = Py_BuildValue("(l)", switch_id);

    call_python(py_switch_shutdown_request_notification, arglist);

    Py_DECREF(arglist);
}

static void sai_switch_state_change_notification(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    PyObject* arglist = Py_BuildValue("(li)", switch_id, switch_oper_status);

    call_python(py_switch_state_change_notification, arglist);

    Py_DECREF(arglist);
}

static void sai_bfd_session_state_change_notification(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    PyObject* obj = py_convert_sai_bfd_session_state_notification_t_to_PyObject(data);

    PyObject* arglist = Py_BuildValue("(iO)", count, obj);

    call_python(py_bfd_session_state_change_notification, arglist);

    Py_DECREF(obj);

    Py_DECREF(arglist);
}

static void sai_tam_tel_type_config_change_notification(
    _In_ sai_object_id_t tam_tel_type_id)
{
    PyObject *arglist = Py_BuildValue("(l)", tam_tel_type_id);

    call_python(py_tam_tel_type_config_change_notification, arglist);

    Py_DECREF(arglist);
}

sai_pointer_t sai_get_notification_pointer(
        sai_attr_id_t id,
        PyObject*callback)
{
    if (!PyCallable_Check(callback))
    {
        SWSS_LOG_THROW("second parameter must be python callable object");
    }

    Py_XINCREF(callback); // inc ref on current callback

    switch(id)
    {
        case SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY:
            Py_XDECREF(py_switch_state_change_notification);
            py_switch_state_change_notification = callback;
            return (void*)&sai_switch_state_change_notification;

        case SAI_SWITCH_ATTR_SWITCH_SHUTDOWN_REQUEST_NOTIFY:
            Py_XDECREF(py_switch_shutdown_request_notification);
            py_switch_shutdown_request_notification = callback;
            return (void*)&sai_switch_shutdown_request_notification;

        case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
            Py_XDECREF(py_fdb_event_notification);
            py_fdb_event_notification = callback;
            return (void*)&sai_fdb_event_notification;

        case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
            Py_XDECREF(py_port_state_change_notification);
            py_port_state_change_notification = callback;
            return (void*)&sai_port_state_change_notification;

        case SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY:
            Py_XDECREF(py_queue_pfc_deadlock_notification);
            py_queue_pfc_deadlock_notification = callback;
            return (void*)&sai_queue_pfc_deadlock_notification;

        case SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY:
            Py_XDECREF(py_bfd_session_state_change_notification);
            py_bfd_session_state_change_notification = callback;
            return (void*)&sai_bfd_session_state_change_notification;

        case SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY:
            Py_XDECREF(py_tam_tel_type_config_change_notification);
            py_tam_tel_type_config_change_notification = callback;
            return (void*)&sai_tam_tel_type_config_change_notification;

        default:
            Py_XDECREF(callback);
            break;
    }

    SWSS_LOG_THROW("notification attr id %d not supported", id);
}
