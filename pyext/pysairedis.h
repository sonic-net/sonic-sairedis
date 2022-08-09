
#include <Python.h>

extern "C" {
#include "sai.h"
}

#include <map>
#include <string>

sai_status_t sai_api_initialize(
        uint64_t flags,
        const std::map<std::string, std::string>& profileMap);

// will take python callback, and return C callback to set on notify attributes
sai_pointer_t sai_get_notification_pointer(
        sai_attr_id_t id,
        PyObject*callback);
