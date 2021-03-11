#include "Python.h"

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#include "../lib/inc/Sai.h"
#include "../meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/tokenize.h"

#include <map>
#include <string>
#include <memory>
#include <vector>

#define MAX_LIST_SIZE (0x1000)
#define LIST_ITEM_MAX_SIZE (sizeof(sai_attribute_t))

static std::map<std::string, std::string> g_profileMap;

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

    static auto it = g_profileMap.begin();

    if (value == NULL)
    {
        // Restarts enumeration
        it = g_profileMap.begin();
    }
    else if (it == g_profileMap.end())
    {
        return -1;
    }
    else
    {
        *variable = it->first.c_str();
        *value = it->second.c_str();
        it++;
    }

    if (it != g_profileMap.end())
        return 0;

    return -1;
}

static const sai_service_method_table_t service_method_table = {
    profile_get_value,
    profile_get_next_value
};

static std::shared_ptr<sairedis::Sai> g_sai;

static PyObject* SaiRedisError;

static bool parse_entry_dict(
        _Inout_ sai_object_meta_key_t& metaKey,
        _In_ PyObject* dict)
{
    SWSS_LOG_ENTER();

    std::map<std::string, std::string> map;

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value))
    {
        if (!PyString_Check(key) || !PyString_Check(value))
        {
            PyErr_Format(SaiRedisError, "Keys and values in dict must be strings");
            return false;
        }

        // save pair to map
        map[PyString_AsString(key)] = PyString_AsString(value);
    }

    switch (metaKey.objecttype)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:

            try
            {
                auto it = map.find("switch_id");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "switch_id missing in route_entry");
                    return false;
                }

                sai_deserialize_object_id(it->second, metaKey.objectkey.key.route_entry.switch_id);

                it = map.find("vr_id");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "vr_id missing in route_entry");
                    return false;
                }

                sai_deserialize_object_id(it->second, metaKey.objectkey.key.route_entry.vr_id);

                it = map.find("destination");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "destination missing in route_entry");
                    return false;
                }

                sai_deserialize_ip_prefix(it->second, metaKey.objectkey.key.route_entry.destination);
            }
            catch (const std::exception& e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize route_entry: %s", e.what());
                return false;
            }

            return true;

        case SAI_OBJECT_TYPE_FDB_ENTRY:

            try
            {
                auto it = map.find("switch_id");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "switch_id missing in fdb_entry");
                    return false;
                }

                sai_deserialize_object_id(it->second, metaKey.objectkey.key.fdb_entry.switch_id);

                it = map.find("bv_id");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "bv_id missing in fdb_entry");
                    return false;
                }

                sai_deserialize_object_id(it->second, metaKey.objectkey.key.fdb_entry.bv_id);

                it = map.find("mac_address");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "destination missing in fdb_entry");
                    return false;
                }

                sai_deserialize_mac(it->second, metaKey.objectkey.key.fdb_entry.mac_address);
            }
            catch (const std::exception& e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize fdb_entry: %s", e.what());
                return false;
            }

            return true;


        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:

            try
            {
                auto it = map.find("switch_id");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "switch_id missing in neighbor_entry");
                    return false;
                }

                sai_deserialize_object_id(it->second, metaKey.objectkey.key.neighbor_entry.switch_id);

                it = map.find("rif_id");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "rif_id missing in neighbor_entry");
                    return false;
                }

                sai_deserialize_object_id(it->second, metaKey.objectkey.key.neighbor_entry.rif_id);

                it = map.find("ip_address");

                if (it == map.end())
                {
                    PyErr_Format(SaiRedisError, "destination missing in neighbor_entry");
                    return false;
                }

                sai_deserialize_ip_address(it->second, metaKey.objectkey.key.neighbor_entry.ip_address);
            }
            catch (const std::exception& e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize neighbor_entry: %s", e.what());
                return false;
            }

            return true;

        default:

            PyErr_Format(SaiRedisError, "Object type %s is not implemented yet, FIXME",
                    sai_serialize_object_type(metaKey.objecttype).c_str());
            return false;
    }
}

static PyObject * generic_create(
        _In_ sai_object_type_t objectType,
        _In_ PyObject *self,
        _In_ PyObject *args)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;

    auto* info = sai_metadata_get_object_type_info(objectType);

    if (!info)
    {
        PyErr_Format(SaiRedisError, "Invalid object type specified");
        return nullptr;
    }

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    sai_object_meta_key_t metaKey;

    metaKey.objecttype = objectType;
    metaKey.objectkey.key.object_id = SAI_NULL_OBJECT_ID;

    int size = (int)PyTuple_Size(args);

    PyObject* dict = nullptr;

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        if (size != 1)
        {
            PyErr_Format(SaiRedisError, "Expected number of arguments is 1, but %d given", size);
            return nullptr;
        }

        dict = PyTuple_GetItem(args, 0);
    }
    else
    {
        if (size != 2)
        {
            PyErr_Format(SaiRedisError, "Expected number of arguments is 2, but %d given", size);
            return nullptr;
        }

        if (info->isobjectid)
        {
            auto*swid = PyTuple_GetItem(args, 0);

            if (!PyString_Check(swid))
            {
                PyErr_Format(SaiRedisError, "Switch id must be string type");
                return nullptr;
            }

            try
            {
                sai_deserialize_object_id(PyString_AsString(swid), &switchId);
            }
            catch (const std::exception&e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize switchId: %s", e.what());
                return nullptr;
            }

            dict = PyTuple_GetItem(args, 1);
        }
        else // non object ID specified
        {
            auto entry_dict = PyTuple_GetItem(args, 0);

            if (!PyDict_CheckExact(entry_dict))
            {
                PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
                return nullptr;
            }

            if (!parse_entry_dict(metaKey, entry_dict))
            {
                return nullptr;
            }

            dict = PyTuple_GetItem(args, 1);
        }
    }

    if (!PyDict_CheckExact(dict))
    {
        PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
        return nullptr;
    }

    std::map<std::string, std::string> map;

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value))
    {
        if (!PyString_Check(key) || !PyString_Check(value))
        {
            PyErr_Format(SaiRedisError, "Keys and values in dict must be strings");
            return nullptr;
        }

        // save pair to map
        map[PyString_AsString(key)] = PyString_AsString(value);
    }

    std::vector<sai_attribute_t> attrs;
    std::vector<const sai_attr_metadata_t*> meta;

    for (auto& kvp: map)
    {
        auto*md = sai_metadata_get_attr_metadata_by_attr_id_name(kvp.first.c_str());

        if (!md)
        {
            PyErr_Format(SaiRedisError, "Invalid attribute: %s", kvp.first.c_str());
            return nullptr;
        }

        if (md->objecttype != objectType)
        {
            PyErr_Format(SaiRedisError, "Attribute: %s is not %s", kvp.first.c_str(), info->objecttypename);
            return nullptr;
        }

        sai_attribute_t attr;

        try
        {
            attr.id = md->attrid;

            sai_deserialize_attr_value(kvp.second, *md, attr, false);
        }
        catch (const std::exception&e)
        {
            PyErr_Format(SaiRedisError, "Failed to deserialize %s '%s': %s", kvp.first.c_str(), kvp.second.c_str(), e.what());
            return nullptr;
        }

        attrs.push_back(attr);
        meta.push_back(md);
    }

    std::shared_ptr<sairedis::SaiInterface> sai = g_sai;

    sai_status_t status = sai->create(
            metaKey,
            switchId,
            (uint32_t)attrs.size(),
            attrs.data());

    for (size_t i = 0; i < attrs.size(); i++)
    {
        // free potentially allocated memory
        sai_deserialize_free_attribute_value(meta[i]->attrvaluetype, attrs[i]);
    }

    PyObject *pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    if (status == SAI_STATUS_SUCCESS && info->isobjectid)
    {
        PyDict_SetItemString(pdict, "oid", PyString_FromFormat("%s",
                    sai_serialize_object_id(metaKey.objectkey.key.object_id).c_str()));
    }

    return pdict;
}

static PyObject * generic_remove(
        _In_ sai_object_type_t objectType,
        _In_ PyObject *self,
        _In_ PyObject *args)
{
    SWSS_LOG_ENTER();

    sai_object_meta_key_t metaKey;

    metaKey.objecttype = objectType;
    metaKey.objectkey.key.object_id = SAI_NULL_OBJECT_ID;

    auto* info = sai_metadata_get_object_type_info(objectType);

    if (!info)
    {
        PyErr_Format(SaiRedisError, "Invalid object type specified");
        return nullptr;
    }

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    int size = (int)PyTuple_Size(args);

    if (size != 1)
    {
        PyErr_Format(SaiRedisError, "Expected number of arguments is 1, but %d given", size);
        return nullptr;
    }

    if (info->isobjectid)
    {
        auto*oid = PyTuple_GetItem(args, 0);

        if (!PyString_Check(oid))
        {
            PyErr_Format(SaiRedisError, "Passed argument must be of type string");
            return nullptr;
        }

        try
        {
            sai_deserialize_object_id(PyString_AsString(oid), &metaKey.objectkey.key.object_id);
        }
        catch (const std::exception&e)
        {
            PyErr_Format(SaiRedisError, "Failed to deserialize switchId: %s", e.what());
            return nullptr;
        }
    }
    else
    {
        auto entry_dict = PyTuple_GetItem(args, 0);

        if (!PyDict_CheckExact(entry_dict))
        {
            PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
            return nullptr;
        }

        if (!parse_entry_dict(metaKey, entry_dict))
        {
            return nullptr;
        }
    }

    std::shared_ptr<sairedis::SaiInterface> sai = g_sai;

    sai_status_t status = sai->remove(metaKey);

    PyObject *pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    return pdict;
}

static PyObject * generic_set(
        _In_ sai_object_type_t objectType,
        _In_ PyObject *self,
        _In_ PyObject *args)
{
    SWSS_LOG_ENTER();

    sai_object_meta_key_t metaKey;

    metaKey.objecttype = objectType;
    metaKey.objectkey.key.object_id = SAI_NULL_OBJECT_ID;

    auto* info = sai_metadata_get_object_type_info(objectType);

    if (!info)
    {
        PyErr_Format(SaiRedisError, "Invalid object type specified");
        return nullptr;
    }

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    int size = (int)PyTuple_Size(args);

    if (size != 3)
    {
        PyErr_Format(SaiRedisError, "Expected number of arguments is 3, but %d given", size);
        return nullptr;
    }

    auto* pyoid = PyTuple_GetItem(args, 0);
    auto* pyattr = PyTuple_GetItem(args, 1);
    auto* pyval = PyTuple_GetItem(args, 2);

    if (!PyString_Check(pyattr) || !PyString_Check(pyval))
    {
        PyErr_Format(SaiRedisError, "All parameters must be of type string");
        return nullptr;
    }

    if (info->isobjectid)
    {
        auto*oid = pyoid;

        if (!PyString_Check(oid))
        {
            PyErr_Format(SaiRedisError, "Passed argument must be of type string");
            return nullptr;
        }

        try
        {
            sai_deserialize_object_id(PyString_AsString(oid), &metaKey.objectkey.key.object_id);
        }
        catch (const std::exception&e)
        {
            PyErr_Format(SaiRedisError, "Failed to deserialize switchId: %s", e.what());
            return nullptr;
        }
    }
    else
    {
        auto entry_dict = pyoid;

        if (!PyDict_CheckExact(entry_dict))
        {
            PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
            return nullptr;
        }

        if (!parse_entry_dict(metaKey, entry_dict))
        {
            return nullptr;
        }
    }

    std::string strAttr = PyString_AsString(pyattr);
    std::string strVal = PyString_AsString(pyval);

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        if (strAttr == "SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT")
        {
            sai_attribute_t attr;

            attr.id = SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT;

            uint32_t value;

            try
            {
                sai_deserialize_number(strVal, value);
            }
            catch (const std::exception&e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize %s '%s': %s", strAttr.c_str(), strVal.c_str(), e.what());
                return nullptr;
            }

            attr.value.u64 = value;

            sai_status_t status = g_sai->set(objectType, metaKey.objectkey.key.object_id, &attr);

            PyObject *pdict = PyDict_New();
            PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

            return pdict;
        }
        else if (strAttr == "SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD")
        {
            sai_attribute_t attr;

            attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;

            sai_redis_notify_syncd_t value;

            try
            {
                sai_deserialize(strVal, value);
            }
            catch (const std::exception&e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize %s '%s': %s", strAttr.c_str(), strVal.c_str(), e.what());
                return nullptr;
            }

            attr.value.s32 = value;

            sai_status_t status = g_sai->set(objectType, metaKey.objectkey.key.object_id, &attr);

            PyObject *pdict = PyDict_New();
            PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

            return pdict;
        }
        else if (strAttr == "SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE")
        {
            sai_attribute_t attr;

            attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;

            sai_redis_communication_mode_t value;

            try
            {
                sai_deserialize_redis_communication_mode(strVal, value);
            }
            catch (const std::exception&e)
            {
                PyErr_Format(SaiRedisError, "Failed to deserialize %s '%s': %s", strAttr.c_str(), strVal.c_str(), e.what());
                return nullptr;
            }

            attr.value.s32 = value;

            sai_status_t status = g_sai->set(objectType, metaKey.objectkey.key.object_id, &attr);

            PyObject *pdict = PyDict_New();
            PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

            return pdict;
        }
    }

    auto*md = sai_metadata_get_attr_metadata_by_attr_id_name(strAttr.c_str());

    if (!md)
    {
        PyErr_Format(SaiRedisError, "Invalid attribute: %s", strAttr.c_str());
        return nullptr;
    }

    if (md->objecttype != objectType)
    {
        PyErr_Format(SaiRedisError, "Attribute: %s is not %s", strAttr.c_str(), info->objecttypename);
        return nullptr;
    }

    sai_attribute_t attr;

    try
    {
        attr.id = md->attrid;

        sai_deserialize_attr_value(strVal, *md, attr, false);
    }
    catch (const std::exception&e)
    {
        PyErr_Format(SaiRedisError, "Failed to deserialize %s '%s': %s", strAttr.c_str(), strVal.c_str(), e.what());
        return nullptr;
    }

    std::shared_ptr<sairedis::SaiInterface> sai = g_sai;

    sai_status_t status = sai->set(metaKey, &attr);

    // free potentially allocated memory
    sai_deserialize_free_attribute_value(md->attrvaluetype, attr);

    PyObject *pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    return pdict;
}

static PyObject * generic_get(
        _In_ sai_object_type_t objectType,
        _In_ PyObject *self,
        _In_ PyObject *args)
{
    SWSS_LOG_ENTER();

    sai_object_meta_key_t metaKey;

    metaKey.objecttype = objectType;
    metaKey.objectkey.key.object_id = SAI_NULL_OBJECT_ID;

    auto* info = sai_metadata_get_object_type_info(objectType);

    if (!info)
    {
        PyErr_Format(SaiRedisError, "Invalid object type specified");
        return nullptr;
    }

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    int size = (int)PyTuple_Size(args);

    if (size != 2)
    {
        PyErr_Format(SaiRedisError, "Expected number of arguments is 2, but %d given", size);
        return nullptr;
    }

    auto*pyoid = PyTuple_GetItem(args, 0);
    auto*pyattr = PyTuple_GetItem(args, 1);

    if (!PyString_Check(pyattr))
    {
        PyErr_Format(SaiRedisError, "All atttributes must be of type string");
        return nullptr;
    }

    if (info->isobjectid)
    {
        auto*oid = pyoid;

        if (!PyString_Check(oid))
        {
            PyErr_Format(SaiRedisError, "Passed argument must be of type string");
            return nullptr;
        }

        try
        {
            sai_deserialize_object_id(PyString_AsString(oid), &metaKey.objectkey.key.object_id);
        }
        catch (const std::exception&e)
        {
            PyErr_Format(SaiRedisError, "Failed to deserialize switchId: %s", e.what());
            return nullptr;
        }
    }
    else
    {
        auto entry_dict = pyoid;

        if (!PyDict_CheckExact(entry_dict))
        {
            PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
            return nullptr;
        }

        if (!parse_entry_dict(metaKey, entry_dict))
        {
            return nullptr;
        }
    }

    std::string strAttr = PyString_AsString(pyattr);

    auto*md = sai_metadata_get_attr_metadata_by_attr_id_name(strAttr.c_str());

    if (!md)
    {
        PyErr_Format(SaiRedisError, "Invalid attribute: %s", strAttr.c_str());
        return nullptr;
    }

    if (md->objecttype != objectType)
    {
        PyErr_Format(SaiRedisError, "Attribute: %s is not %s", strAttr.c_str(), info->objecttypename);
        return nullptr;
    }

    // we need to populate list pointers with predefined max value
    // so user will not have to manually populate that from python
    // all data types with pointers need special handling

    sai_attribute_t attr = {};

    std::vector<int> data(MAX_LIST_SIZE * LIST_ITEM_MAX_SIZE);

    switch (md->attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_BOOL:
        case SAI_ATTR_VALUE_TYPE_CHARDATA:
        case SAI_ATTR_VALUE_TYPE_UINT8:
        case SAI_ATTR_VALUE_TYPE_INT8:
        case SAI_ATTR_VALUE_TYPE_UINT16:
        case SAI_ATTR_VALUE_TYPE_INT16:
        case SAI_ATTR_VALUE_TYPE_UINT32:
        case SAI_ATTR_VALUE_TYPE_INT32:
        case SAI_ATTR_VALUE_TYPE_UINT64:
        case SAI_ATTR_VALUE_TYPE_INT64:
        case SAI_ATTR_VALUE_TYPE_MAC:
        case SAI_ATTR_VALUE_TYPE_IPV4:
        case SAI_ATTR_VALUE_TYPE_IPV6:
        case SAI_ATTR_VALUE_TYPE_POINTER:
        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS:
        case SAI_ATTR_VALUE_TYPE_IP_PREFIX:
        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
            break;

        case SAI_ATTR_VALUE_TYPE_UINT32_RANGE:
        case SAI_ATTR_VALUE_TYPE_INT32_RANGE:
            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
        case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
        case SAI_ATTR_VALUE_TYPE_INT8_LIST:
        case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
        case SAI_ATTR_VALUE_TYPE_INT16_LIST:
        case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
        case SAI_ATTR_VALUE_TYPE_INT32_LIST:
        case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
        case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
        case SAI_ATTR_VALUE_TYPE_MAP_LIST:
        case SAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
        case SAI_ATTR_VALUE_TYPE_TLV_LIST:
        case SAI_ATTR_VALUE_TYPE_SEGMENT_LIST:
        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
        case SAI_ATTR_VALUE_TYPE_PORT_EYE_VALUES_LIST:
            // we can get away with this since each list is count + pointer
            attr.value.s32list.count = MAX_LIST_SIZE;
            attr.value.s32list.list = data.data();
            break;

            // ACL FIELD DATA

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            break;

            //case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            //case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:

            // ACL ACTION DATA

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            break;

            //case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            //case SAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:

        default:
            PyErr_Format(SaiRedisError, "Attribute: %s value type is not supported yet, FIXME", strAttr.c_str());
            return nullptr;
    }

    attr.id = md->attrid;

    std::shared_ptr<sairedis::SaiInterface> sai = g_sai;

    sai_status_t status = sai->get(
            metaKey,
            1,
            &attr);

    PyObject *pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    if (status == SAI_STATUS_SUCCESS)
    {
        switch (md->attrvaluetype)
        {
            // for list's we want to split list and return actual list
            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
            case SAI_ATTR_VALUE_TYPE_INT8_LIST:
            case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
            case SAI_ATTR_VALUE_TYPE_INT16_LIST:
            case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
            case SAI_ATTR_VALUE_TYPE_INT32_LIST:
            case SAI_ATTR_VALUE_TYPE_VLAN_LIST:

                // for simple lists, where they are separated by ',' we can use this split
                {
                    auto val = sai_serialize_attr_value(*md, attr, false);

                    val = val.substr(val.find_first_of(":") + 1);

                    auto tokens = swss::tokenize(val, ',');

                    auto *list = PyList_New(0);

                    for (auto&tok: tokens)
                    {
                        PyList_Append(list, PyString_FromString(tok.c_str()));
                    }

                    PyDict_SetItemString(pdict, strAttr.c_str(), list);
                }

                break;

            default:

                PyDict_SetItemString(pdict, strAttr.c_str(), PyString_FromFormat("%s", sai_serialize_attr_value(*md, attr, false).c_str()));
                break;
        }
    }

    return pdict;
}

// GLOBAL SAI APIS

static PyObject* api_initialize(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    int size = (int)PyTuple_Size(args);

    PyObject* dict = nullptr;

    if (size != 1)
    {
        PyErr_Format(SaiRedisError, "Expected number of arguments is 1, but %d given", size);
        return nullptr;
    }

    dict = PyTuple_GetItem(args, 0);

    if (!PyDict_CheckExact(dict))
    {
        PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
        return nullptr;
    }

    g_profileMap.clear();

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value))
    {
        if (!PyString_Check(key) || !PyString_Check(value))
        {
            PyErr_Format(SaiRedisError, "Keys and values in dict must be strings");
            return nullptr;
        }

        g_profileMap[PyString_AsString(key)] = PyString_AsString(value);
    }

    auto status = g_sai->initialize(0, &service_method_table);

    PyObject *pdict = PyDict_New();

    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    return pdict;
}

static PyObject* api_uninitialize(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    auto status = g_sai->uninitialize();

    PyObject *pdict = PyDict_New();

    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    return pdict;
}

// QUAD API

#define PYTHON_CREATE(OT,ot) \
static PyObject* create_ ## ot(PyObject *self, PyObject *args) \
{ return generic_create(SAI_OBJECT_TYPE_ ## OT , self, args); }

#define PYTHON_REMOVE(OT, ot) \
static PyObject* remove_ ## ot(PyObject *self, PyObject *args) \
{ return generic_remove(SAI_OBJECT_TYPE_ ## OT, self, args); }

#define PYTHON_SET(OT,ot) \
static PyObject* set_ ## ot ## _attribute(PyObject *self, PyObject *args) \
{ return generic_set(SAI_OBJECT_TYPE_ ## OT, self, args); }

#define PYTHON_GET(OT,ot) \
static PyObject* get_ ## ot ## _attribute(PyObject *self, PyObject *args) \
{ return generic_get(SAI_OBJECT_TYPE_ ## OT, self, args); }

#define PYTHON_GENERIC_QUAD(OT,ot) \
    PYTHON_CREATE(OT,ot) \
    PYTHON_REMOVE(OT,ot) \
    PYTHON_SET(OT,ot) \
    PYTHON_GET(OT,ot)

PYTHON_GENERIC_QUAD(PORT,port);
PYTHON_GENERIC_QUAD(LAG,lag);
PYTHON_GENERIC_QUAD(VIRTUAL_ROUTER,virtual_router);
PYTHON_GENERIC_QUAD(NEXT_HOP,next_hop);
PYTHON_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
PYTHON_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);
PYTHON_GENERIC_QUAD(ACL_TABLE,acl_table);
PYTHON_GENERIC_QUAD(ACL_ENTRY,acl_entry);
PYTHON_GENERIC_QUAD(ACL_COUNTER,acl_counter);
PYTHON_GENERIC_QUAD(ACL_RANGE,acl_range);
PYTHON_GENERIC_QUAD(ACL_TABLE_GROUP,acl_table_group);
PYTHON_GENERIC_QUAD(ACL_TABLE_GROUP_MEMBER,acl_table_group_member);
PYTHON_GENERIC_QUAD(HOSTIF,hostif);
PYTHON_GENERIC_QUAD(MIRROR_SESSION,mirror_session);
PYTHON_GENERIC_QUAD(SAMPLEPACKET,samplepacket);
PYTHON_GENERIC_QUAD(STP,stp);
PYTHON_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
PYTHON_GENERIC_QUAD(POLICER,policer);
PYTHON_GENERIC_QUAD(WRED,wred);
PYTHON_GENERIC_QUAD(QOS_MAP,qos_map);
PYTHON_GENERIC_QUAD(QUEUE,queue);
PYTHON_GENERIC_QUAD(SCHEDULER,scheduler);
PYTHON_GENERIC_QUAD(SCHEDULER_GROUP,scheduler_group);
PYTHON_GENERIC_QUAD(BUFFER_POOL,buffer_pool);
PYTHON_GENERIC_QUAD(BUFFER_PROFILE,buffer_profile);
PYTHON_GENERIC_QUAD(INGRESS_PRIORITY_GROUP,ingress_priority_group);
PYTHON_GENERIC_QUAD(LAG_MEMBER,lag_member);
PYTHON_GENERIC_QUAD(HASH,hash);
PYTHON_GENERIC_QUAD(UDF,udf);
PYTHON_GENERIC_QUAD(UDF_MATCH,udf_match);
PYTHON_GENERIC_QUAD(UDF_GROUP,udf_group);
PYTHON_GENERIC_QUAD(FDB_ENTRY,fdb_entry);
PYTHON_GENERIC_QUAD(SWITCH,switch);
PYTHON_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
PYTHON_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
PYTHON_GENERIC_QUAD(NEIGHBOR_ENTRY,neighbor_entry);
PYTHON_GENERIC_QUAD(ROUTE_ENTRY,route_entry);
PYTHON_GENERIC_QUAD(VLAN,vlan);
PYTHON_GENERIC_QUAD(VLAN_MEMBER,vlan_member);
PYTHON_GENERIC_QUAD(HOSTIF_PACKET,hostif_packet);
PYTHON_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
PYTHON_GENERIC_QUAD(TUNNEL,tunnel);
PYTHON_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
PYTHON_GENERIC_QUAD(FDB_FLUSH,fdb_flush);
PYTHON_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);
PYTHON_GENERIC_QUAD(STP_PORT,stp_port);
PYTHON_GENERIC_QUAD(RPF_GROUP,rpf_group);
PYTHON_GENERIC_QUAD(RPF_GROUP_MEMBER,rpf_group_member);
PYTHON_GENERIC_QUAD(L2MC_GROUP,l2mc_group);
PYTHON_GENERIC_QUAD(L2MC_GROUP_MEMBER,l2mc_group_member);
PYTHON_GENERIC_QUAD(IPMC_GROUP,ipmc_group);
PYTHON_GENERIC_QUAD(IPMC_GROUP_MEMBER,ipmc_group_member);
PYTHON_GENERIC_QUAD(L2MC_ENTRY,l2mc_entry);
PYTHON_GENERIC_QUAD(IPMC_ENTRY,ipmc_entry);
PYTHON_GENERIC_QUAD(MCAST_FDB_ENTRY,mcast_fdb_entry);
PYTHON_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);
PYTHON_GENERIC_QUAD(BRIDGE,bridge);
PYTHON_GENERIC_QUAD(BRIDGE_PORT,bridge_port);
PYTHON_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);
PYTHON_GENERIC_QUAD(TAM,tam);
PYTHON_GENERIC_QUAD(SEGMENTROUTE_SIDLIST,segmentroute_sidlist);
PYTHON_GENERIC_QUAD(PORT_POOL,port_pool);
PYTHON_GENERIC_QUAD(INSEG_ENTRY,inseg_entry);
PYTHON_GENERIC_QUAD(DTEL,dtel);
PYTHON_GENERIC_QUAD(DTEL_QUEUE_REPORT,dtel_queue_report);
PYTHON_GENERIC_QUAD(DTEL_INT_SESSION,dtel_int_session);
PYTHON_GENERIC_QUAD(DTEL_REPORT_SESSION,dtel_report_session);
PYTHON_GENERIC_QUAD(DTEL_EVENT,dtel_event);
PYTHON_GENERIC_QUAD(BFD_SESSION,bfd_session);
PYTHON_GENERIC_QUAD(ISOLATION_GROUP,isolation_group);
PYTHON_GENERIC_QUAD(ISOLATION_GROUP_MEMBER,isolation_group_member);
PYTHON_GENERIC_QUAD(TAM_MATH_FUNC,tam_math_func);
PYTHON_GENERIC_QUAD(TAM_REPORT,tam_report);
PYTHON_GENERIC_QUAD(TAM_EVENT_THRESHOLD,tam_event_threshold);
PYTHON_GENERIC_QUAD(TAM_TEL_TYPE,tam_tel_type);
PYTHON_GENERIC_QUAD(TAM_TRANSPORT,tam_transport);
PYTHON_GENERIC_QUAD(TAM_TELEMETRY,tam_telemetry);
PYTHON_GENERIC_QUAD(TAM_COLLECTOR,tam_collector);
PYTHON_GENERIC_QUAD(TAM_EVENT_ACTION,tam_event_action);
PYTHON_GENERIC_QUAD(TAM_EVENT,tam_event);
PYTHON_GENERIC_QUAD(NAT_ZONE_COUNTER,nat_zone_counter);
PYTHON_GENERIC_QUAD(NAT_ENTRY,nat_entry);
PYTHON_GENERIC_QUAD(TAM_INT,tam_int);
PYTHON_GENERIC_QUAD(COUNTER,counter);
PYTHON_GENERIC_QUAD(DEBUG_COUNTER,debug_counter);
PYTHON_GENERIC_QUAD(PORT_CONNECTOR,port_connector);
PYTHON_GENERIC_QUAD(PORT_SERDES,port_serdes);
PYTHON_GENERIC_QUAD(MACSEC,macsec);
PYTHON_GENERIC_QUAD(MACSEC_PORT,macsec_port);
PYTHON_GENERIC_QUAD(MACSEC_FLOW,macsec_flow);
PYTHON_GENERIC_QUAD(MACSEC_SC,macsec_sc);
PYTHON_GENERIC_QUAD(MACSEC_SA,macsec_sa);
PYTHON_GENERIC_QUAD(SYSTEM_PORT,system_port);
PYTHON_GENERIC_QUAD(FINE_GRAINED_HASH_FIELD,fine_grained_hash_field);

#define PYTHON_METHODS_GENERIC_QUAD(ot) \
{"create_" # ot,   create_ ## ot,  METH_VARARGS, "Create " # ot }, \
{"remove_" # ot,   remove_ ## ot,  METH_VARARGS, "Remove " # ot }, \
{"set_" # ot "_attribute",      set_ ## ot ## _attribute,     METH_VARARGS, "Set " # ot " atribute."}, \
{"get_" # ot "_attribute",      get_ ## ot ## _attribute,     METH_VARARGS, "Get " # ot " atribute."},

static PyMethodDef SaiRedisMethods[] = {

    {"api_initialize",      api_initialize,     METH_VARARGS, "SAI initialize"},
    {"api_uninitialize",    api_uninitialize,   METH_VARARGS, "SAI uninitialize"},

    PYTHON_METHODS_GENERIC_QUAD(port)
    PYTHON_METHODS_GENERIC_QUAD(lag)
    PYTHON_METHODS_GENERIC_QUAD(virtual_router)
    PYTHON_METHODS_GENERIC_QUAD(next_hop)
    PYTHON_METHODS_GENERIC_QUAD(next_hop_group)
    PYTHON_METHODS_GENERIC_QUAD(router_interface)
    PYTHON_METHODS_GENERIC_QUAD(acl_table)
    PYTHON_METHODS_GENERIC_QUAD(acl_entry)
    PYTHON_METHODS_GENERIC_QUAD(acl_counter)
    PYTHON_METHODS_GENERIC_QUAD(acl_range)
    PYTHON_METHODS_GENERIC_QUAD(acl_table_group)
    PYTHON_METHODS_GENERIC_QUAD(acl_table_group_member)
    PYTHON_METHODS_GENERIC_QUAD(hostif)
    PYTHON_METHODS_GENERIC_QUAD(mirror_session)
    PYTHON_METHODS_GENERIC_QUAD(samplepacket)
    PYTHON_METHODS_GENERIC_QUAD(stp)
    PYTHON_METHODS_GENERIC_QUAD(hostif_trap_group)
    PYTHON_METHODS_GENERIC_QUAD(policer)
    PYTHON_METHODS_GENERIC_QUAD(wred)
    PYTHON_METHODS_GENERIC_QUAD(qos_map)
    PYTHON_METHODS_GENERIC_QUAD(queue)
    PYTHON_METHODS_GENERIC_QUAD(scheduler)
    PYTHON_METHODS_GENERIC_QUAD(scheduler_group)
    PYTHON_METHODS_GENERIC_QUAD(buffer_pool)
    PYTHON_METHODS_GENERIC_QUAD(buffer_profile)
    PYTHON_METHODS_GENERIC_QUAD(ingress_priority_group)
    PYTHON_METHODS_GENERIC_QUAD(lag_member)
    PYTHON_METHODS_GENERIC_QUAD(hash)
    PYTHON_METHODS_GENERIC_QUAD(udf)
    PYTHON_METHODS_GENERIC_QUAD(udf_match)
    PYTHON_METHODS_GENERIC_QUAD(udf_group)
    PYTHON_METHODS_GENERIC_QUAD(fdb_entry)
    PYTHON_METHODS_GENERIC_QUAD(switch)
    PYTHON_METHODS_GENERIC_QUAD(hostif_trap)
    PYTHON_METHODS_GENERIC_QUAD(hostif_table_entry)
    PYTHON_METHODS_GENERIC_QUAD(neighbor_entry)
    PYTHON_METHODS_GENERIC_QUAD(route_entry)
    PYTHON_METHODS_GENERIC_QUAD(vlan)
    PYTHON_METHODS_GENERIC_QUAD(vlan_member)
    PYTHON_METHODS_GENERIC_QUAD(hostif_packet)
    PYTHON_METHODS_GENERIC_QUAD(tunnel_map)
    PYTHON_METHODS_GENERIC_QUAD(tunnel)
    PYTHON_METHODS_GENERIC_QUAD(tunnel_term_table_entry)
    PYTHON_METHODS_GENERIC_QUAD(fdb_flush)
    PYTHON_METHODS_GENERIC_QUAD(next_hop_group_member)
    PYTHON_METHODS_GENERIC_QUAD(stp_port)
    PYTHON_METHODS_GENERIC_QUAD(rpf_group)
    PYTHON_METHODS_GENERIC_QUAD(rpf_group_member)
    PYTHON_METHODS_GENERIC_QUAD(l2mc_group)
    PYTHON_METHODS_GENERIC_QUAD(l2mc_group_member)
    PYTHON_METHODS_GENERIC_QUAD(ipmc_group)
    PYTHON_METHODS_GENERIC_QUAD(ipmc_group_member)
    PYTHON_METHODS_GENERIC_QUAD(l2mc_entry)
    PYTHON_METHODS_GENERIC_QUAD(ipmc_entry)
    PYTHON_METHODS_GENERIC_QUAD(mcast_fdb_entry)
    PYTHON_METHODS_GENERIC_QUAD(hostif_user_defined_trap)
    PYTHON_METHODS_GENERIC_QUAD(bridge)
    PYTHON_METHODS_GENERIC_QUAD(bridge_port)
    PYTHON_METHODS_GENERIC_QUAD(tunnel_map_entry)
    PYTHON_METHODS_GENERIC_QUAD(tam)
    PYTHON_METHODS_GENERIC_QUAD(segmentroute_sidlist)
    PYTHON_METHODS_GENERIC_QUAD(port_pool)
    PYTHON_METHODS_GENERIC_QUAD(inseg_entry)
    PYTHON_METHODS_GENERIC_QUAD(dtel)
    PYTHON_METHODS_GENERIC_QUAD(dtel_queue_report)
    PYTHON_METHODS_GENERIC_QUAD(dtel_int_session)
    PYTHON_METHODS_GENERIC_QUAD(dtel_report_session)
    PYTHON_METHODS_GENERIC_QUAD(dtel_event)
    PYTHON_METHODS_GENERIC_QUAD(bfd_session)
    PYTHON_METHODS_GENERIC_QUAD(isolation_group)
    PYTHON_METHODS_GENERIC_QUAD(isolation_group_member)
    PYTHON_METHODS_GENERIC_QUAD(tam_math_func)
    PYTHON_METHODS_GENERIC_QUAD(tam_report)
    PYTHON_METHODS_GENERIC_QUAD(tam_event_threshold)
    PYTHON_METHODS_GENERIC_QUAD(tam_tel_type)
    PYTHON_METHODS_GENERIC_QUAD(tam_transport)
    PYTHON_METHODS_GENERIC_QUAD(tam_telemetry)
    PYTHON_METHODS_GENERIC_QUAD(tam_collector)
    PYTHON_METHODS_GENERIC_QUAD(tam_event_action)
    PYTHON_METHODS_GENERIC_QUAD(tam_event)
    PYTHON_METHODS_GENERIC_QUAD(nat_zone_counter)
    PYTHON_METHODS_GENERIC_QUAD(nat_entry)
    PYTHON_METHODS_GENERIC_QUAD(tam_int)
    PYTHON_METHODS_GENERIC_QUAD(counter)
    PYTHON_METHODS_GENERIC_QUAD(debug_counter)
    PYTHON_METHODS_GENERIC_QUAD(port_connector)
    PYTHON_METHODS_GENERIC_QUAD(port_serdes)
    PYTHON_METHODS_GENERIC_QUAD(macsec)
    PYTHON_METHODS_GENERIC_QUAD(macsec_port)
    PYTHON_METHODS_GENERIC_QUAD(macsec_flow)
    PYTHON_METHODS_GENERIC_QUAD(macsec_sc)
    PYTHON_METHODS_GENERIC_QUAD(macsec_sa)
    PYTHON_METHODS_GENERIC_QUAD(system_port)
    PYTHON_METHODS_GENERIC_QUAD(fine_grained_hash_field)

    {NULL, NULL, 0, NULL}        // sentinel
};

extern "C" PyMODINIT_FUNC initsairedis(void);

PyMODINIT_FUNC initsairedis(void)
{
    SWSS_LOG_ENTER();

    PyObject *m;

    m = Py_InitModule("sairedis", SaiRedisMethods);

    if (m == NULL)
        return;

    g_sai = std::make_shared<sairedis::Sai>();

    SaiRedisError = PyErr_NewException(const_cast<char*>("sairedis.error"), NULL, NULL);
    Py_INCREF(SaiRedisError);
    PyModule_AddObject(m, "error", SaiRedisError);
}
