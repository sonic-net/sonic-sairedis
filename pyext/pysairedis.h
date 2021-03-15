#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

extern "C" {
#include "sai.h"
#include "getapi.h"
}

#include <map>
#include <string>

sai_status_t sai_api_initialize(
        uint64_t flags,
        const std::map<std::string, std::string>& profileMap);

