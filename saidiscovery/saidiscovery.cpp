#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>

extern "C" {
#include <sai.h>
#include "saimetadata.h"
}

#include "swss/logger.h"
#include "meta/sai_serialize.h"

#include "meta/Meta.h"
#include "syncd/VendorSai.h"

#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <chrono>

// TODO split to multiple cpp

/**
 * @def MAX_ELEMENTS
 *
 * Defines maximum elements that will be queried when performing
 * sai_object_list_t attribute query.
 */
#define MAX_ELEMENTS 1024

struct cmdOptions
{
    cmdOptions()
    {
        logWarnings        = false;
        initSwitch         = true;
        enableDebugLogs    = false;
        dumpObjects        = false;
        fullDiscovery      = false;
        saiApiLogLevel     = SAI_LOG_LEVEL_NOTICE;
    }

    std::string profileMapFile;
    bool logWarnings;
    bool initSwitch;
    bool enableDebugLogs;
    bool fullDiscovery;
    bool dumpObjects;
    sai_log_level_t saiApiLogLevel;
};

static cmdOptions gOptions;

typedef std::chrono::duration<double, std::ratio<1>> second_t;

/**
 * @brief Discover all objects on given object ID.
 *
 * This method is only good after switch init since we are making
 * assumptions that there are no user created objects after initialization,
 * like ACL and other objects we can discover using this approach. If
 * vendor will support sai_get_object_count and sai_get_object_key then
 * alter on we can use those methods.
 *
 * @param[in] id Object ID to be examined.
 * @param[inout] discovered Map of already discovered objects. Map will be
 * updated if new object will be found.
 *
 * @return Number of calls performed to SAI.
 */
static int discover(
        _In_ std::shared_ptr<sairedis::SaiInterface> sai,
        _In_ sai_object_id_t id,
        _Inout_ std::map<sai_object_id_t, std::map<std::string, std::string>> &discovered)
{
    SWSS_LOG_ENTER();

    int callCount = 0;

    if (id == SAI_NULL_OBJECT_ID)
    {
        return callCount;
    }

    if (discovered.find(id) != discovered.end())
    {
        return callCount;
    }

    sai_object_type_t ot = sai->objectTypeQuery(id);

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("oid %s returned NULL object type",
                sai_serialize_object_id(id).c_str());

        return callCount;
    }

    SWSS_LOG_INFO("processing %s: %s",
            sai_serialize_object_type(ot).c_str(),
            sai_serialize_object_id(id).c_str());

    discovered[id] = {};

    const sai_object_type_info_t *info = sai_metadata_all_object_type_infos[ot];

    for (int idx = 0; info->attrmetadata[idx] != NULL; ++idx)
    {
        const sai_attr_metadata_t *md = info->attrmetadata[idx];

        if (md->objecttype == SAI_OBJECT_TYPE_PORT &&
                md->attrid == SAI_PORT_ATTR_HW_LANE_LIST)
        {
            // XXX workaround for brcm
            continue;
        }

        /*
         * Note that we don't care about ACL object id's since we assume that
         * there are no ACLs on switch after init.
         */

        sai_attribute_t attr;

        attr.id = md->attrid;

        if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            if (md->objecttype == SAI_OBJECT_TYPE_STP &&
                    md->attrid == SAI_STP_ATTR_BRIDGE_ID)
            {
                SWSS_LOG_WARN("skipping %s since it causes crash", md->attridname);
                continue;
            }

            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(id).c_str());

            callCount++;

            sai_status_t status = sai->get(ot, id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                if (gOptions.logWarnings)
                {
                    SWSS_LOG_WARN("%s: %s",
                            md->attridname,
                            sai_serialize_status(status).c_str());
                }

                continue;
            }

            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_CONST
                    && attr.value.oid != SAI_NULL_OBJECT_ID)
            {
                if (gOptions.logWarnings)
                {
                    SWSS_LOG_WARN("const null, but got value %s on %s",
                            sai_serialize_object_id(attr.value.oid).c_str(),
                            md->attridname);
                }
            }

            if (!md->allownullobjectid && attr.value.oid == SAI_NULL_OBJECT_ID)
            {
                if (gOptions.logWarnings)
                {
                    SWSS_LOG_WARN("dont allow null, but got null on %s", md->attridname);
                }
            }

            discovered[id][md->attridname] = sai_serialize_attr_value(*md, attr);

            SWSS_LOG_DEBUG("result on %s: %s: %s",
                    sai_serialize_object_id(id).c_str(),
                    md->attridname,
                    discovered[id][md->attridname].c_str());

            callCount += discover(sai, attr.value.oid, discovered); // recursion
        }
        else if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(id).c_str());

            sai_object_id_t list[MAX_ELEMENTS];

            attr.value.objlist.count = MAX_ELEMENTS;
            attr.value.objlist.list = list;

            callCount++;

            sai_status_t status = sai->get(ot, id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                if (gOptions.logWarnings)
                {
                    SWSS_LOG_WARN("%s: %s",
                            md->attridname,
                            sai_serialize_status(status).c_str());
                }

                continue;
            }

            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST
                    && attr.value.objlist.count != 0)
            {
                if (gOptions.logWarnings)
                {
                    SWSS_LOG_WARN("default is empty list, but got count %u on %s",
                            attr.value.objlist.count,
                            md->attridname);
                }
            }

            discovered[id][md->attridname] = sai_serialize_attr_value(*md, attr);

            SWSS_LOG_INFO("list count %s: %u", md->attridname, attr.value.objlist.count);

            SWSS_LOG_DEBUG("result on %s: %s: %s",
                    sai_serialize_object_id(id).c_str(),
                    md->attridname,
                    discovered[id][md->attridname].c_str());

            for (uint32_t i = 0; i < attr.value.objlist.count; ++i)
            {
                callCount += discover(sai, attr.value.objlist.list[i], discovered); // recursion
            }
        }
        else
        {
            if (!gOptions.fullDiscovery)
            {
                continue;
            }

            if ((md->objecttype == SAI_OBJECT_TYPE_PORT && md->attrid == SAI_PORT_ATTR_FEC_MODE) ||
                (md->objecttype == SAI_OBJECT_TYPE_PORT && md->attrid == SAI_PORT_ATTR_GLOBAL_FLOW_CONTROL_MODE) ||
                (md->objecttype == SAI_OBJECT_TYPE_SWITCH && md->attrid == SAI_SWITCH_ATTR_INIT_SWITCH))
            {
                // workaround since return invalid values
                continue;
            }

            /*
             * Discover non oid attributes as well.
             *
             * TODO lists!
             */

            sai_object_id_t list[MAX_ELEMENTS];

            switch (md->attrvaluetype)
            {
                case SAI_ATTR_VALUE_TYPE_INT8:
                case SAI_ATTR_VALUE_TYPE_INT16:
                case SAI_ATTR_VALUE_TYPE_INT32:
                case SAI_ATTR_VALUE_TYPE_INT64:
                case SAI_ATTR_VALUE_TYPE_UINT8:
                case SAI_ATTR_VALUE_TYPE_UINT16:
                case SAI_ATTR_VALUE_TYPE_UINT32:
                case SAI_ATTR_VALUE_TYPE_UINT64:
                case SAI_ATTR_VALUE_TYPE_POINTER:
                case SAI_ATTR_VALUE_TYPE_BOOL:
                case SAI_ATTR_VALUE_TYPE_UINT16_RANGE:
                case SAI_ATTR_VALUE_TYPE_UINT32_RANGE:
                case SAI_ATTR_VALUE_TYPE_MAC:
                    break;

                case SAI_ATTR_VALUE_TYPE_INT8_LIST:
                case SAI_ATTR_VALUE_TYPE_INT32_LIST:
                case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
                case SAI_ATTR_VALUE_TYPE_VLAN_LIST:

                    attr.value.objlist.count = MAX_ELEMENTS;
                    attr.value.objlist.list = list;
                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:

                    attr.value.aclcapability.action_list.count = MAX_ELEMENTS;
                    attr.value.aclcapability.action_list.list = (int32_t*)list;
                    break;

                default:

                    SWSS_LOG_WARN("attr value: %s not supported",
                             sai_serialize_attr_value_type(md->attrvaluetype).c_str());

                    continue;
            }

            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(id).c_str());

            callCount++;

            sai_status_t status = sai->get(ot, id, 1, &attr);

            if (status == SAI_STATUS_SUCCESS)
            {
                discovered[id][md->attridname] = sai_serialize_attr_value(*md, attr);

                SWSS_LOG_DEBUG("result on %s: %s: %s",
                        sai_serialize_object_id(id).c_str(),
                        md->attridname,
                        discovered[id][md->attridname].c_str());
            }
            else
            {
                if (gOptions.logWarnings)
                {
                    SWSS_LOG_WARN("%s: %s", md->attridname, sai_serialize_status(status).c_str());
                }
            }
        }
    }

    return callCount;
}

static std::map<std::string, std::string> gProfileMap;

static std::map<std::string, std::string>::iterator gProfileIter = gProfileMap.begin();

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = gProfileMap.find(variable);

    if (it == gProfileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        gProfileIter = gProfileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (gProfileIter == gProfileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = gProfileIter->first.c_str();
    *value = gProfileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    gProfileIter++;

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

static void handleProfileMap(
        _In_ const std::string& profileMapFile)
{
    SWSS_LOG_ENTER();

    if (profileMapFile.size() == 0)
    {
        return;
    }

    std::ifstream profile(profileMapFile);

    if (!profile.is_open())
    {
        SWSS_LOG_ERROR("failed to open profile map file: '%s' : %s",
                profileMapFile.c_str(), strerror(errno));

        exit(EXIT_FAILURE);
    }

    std::string line;

    while (getline(profile, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        size_t pos = line.find("=");

        if (pos == std::string::npos)
        {
            SWSS_LOG_WARN("not found '=' in line %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        gProfileMap[key] = value;

        SWSS_LOG_INFO("inserted: %s:%s", key.c_str(), value.c_str());
    }
}

static void printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << std::endl;
    std::cout << "Usage: saidiscovery [-I] [-D] [-f] [-d] [-p profile] [-w] [-h]" << std::endl << std::endl;
    std::cout << "    -I --noInitSwitch:" << std::endl;
    std::cout << "        Try connect to SDK instead of performing init" << std::endl;
    std::cout << "    -D --dumpObjects:" << std::endl;
    std::cout << "        Print each objects and it's attributes" << std::endl;
    std::cout << "    -d --enableDebugLogs:" << std::endl;
    std::cout << "        Enable debug logs" << std::endl;
    std::cout << "    -f --fullDiscovery:" << std::endl;
    std::cout << "        Discover all attributes, not only OIDs" << std::endl;
    std::cout << "    -p --profile profile:" << std::endl;
    std::cout << "        Provide profile map file" << std::endl;
    std::cout << "    -w --logWarnings" << std::endl;
    std::cout << "        Logs all warnings" << std::endl;
    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl << std::endl;
}

static void handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    static struct option long_options[] =
    {
        { "dumpObjects",      no_argument,       0, 'D' },
        { "fullDiscovery",    no_argument,       0, 'f' },
        { "enableDebugLogs",  no_argument,       0, 'd' },
        { "logWarnings",      no_argument,       0, 'w' },
        { "noInitSwitch",     no_argument,       0, 'I' },
        { "profile",          required_argument, 0, 'p' },
        { "help",             no_argument,       0, 'h' },
        { 0,                  0,                 0,  0  }
    };

    const char* const optstring = "DdwIp:hf";

    while (true)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'f':
                gOptions.fullDiscovery = true;
                break;

            case 'D':
                gOptions.dumpObjects = true;
                break;

            case 'd':
                gOptions.enableDebugLogs = true;
                swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);
                gOptions.saiApiLogLevel = SAI_LOG_LEVEL_DEBUG;
                break;

            case 'w':
                gOptions.logWarnings = true;
                break;

            case 'I':
                gOptions.initSwitch = false;
                break;

            case 'p':
                gOptions.profileMapFile = std::string(optarg);
                break;

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    handleCmdLine(argc, argv);

    handleProfileMap(gOptions.profileMapFile);

    auto vendorSai = std::make_shared<syncd::VendorSai>();

    auto sai = std::make_shared<saimeta::Meta>(vendorSai);

    sai_status_t status = sai->apiInitialize(0, (sai_service_method_table_t*)&test_services);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to initialize api: %s",
                sai_serialize_status(status).c_str());

        exit(EXIT_FAILURE);
    }

    for (int api = 1; api < SAI_API_MAX; api++)
    {
        sai->logSet((sai_api_t)api, gOptions.saiApiLogLevel);
    }

    sai_object_id_t switch_id;

    const uint32_t AttributesCount = 1;

    sai_attribute_t attrs[AttributesCount];

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = gOptions.initSwitch;

    SWSS_LOG_NOTICE("creating switch");

    status = sai->create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, AttributesCount, attrs);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to create a switch: %s",
                sai_serialize_status(status).c_str());

        exit(EXIT_FAILURE);
    }

    if (sai->objectTypeQuery(switch_id) != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_ERROR("create switch returned invalid oid: %s",
                sai_serialize_object_id(switch_id).c_str());

        exit(EXIT_FAILURE);
    }

    std::map<sai_object_id_t, std::map<std::string, std::string>> discovered;

    auto m_start = std::chrono::high_resolution_clock::now();

    int callCount = discover(sai, switch_id, discovered);

    auto end = std::chrono::high_resolution_clock::now();

    double duration = std::chrono::duration_cast<second_t>(end - m_start).count();

    SWSS_LOG_NOTICE("discovered objects: %zu took %.3lf sec, call count: %d",
            discovered.size(), duration, callCount);

    std::map<sai_object_type_t, int> map;

    for (const auto &p: discovered)
    {
        map[sai->objectTypeQuery(p.first)]++;
    }

    for (const auto &p: map)
    {
        SWSS_LOG_NOTICE("%s: %d", sai_serialize_object_type(p.first).c_str(), p.second);

        printf("%s: %d\n", sai_serialize_object_type(p.first).c_str(), p.second);
    }

    if (gOptions.dumpObjects)
    {
        printf("\n");

        for (const auto &o: map)
        {
            for (const auto &p: discovered)
            {
                sai_object_type_t ot = sai->objectTypeQuery(p.first);

                if (ot != o.first)
                {
                    continue;
                }

                printf("%s: %s\n",
                        sai_serialize_object_type(ot).c_str(),
                        sai_serialize_object_id(p.first).c_str());

                for (const auto &m: p.second)
                {
                    printf("    %s: %s\n", m.first.c_str(), m.second.c_str());
                }

                printf("\n");
            }
        }
    }

    SWSS_LOG_NOTICE("remove switch");

    status = sai->remove(SAI_OBJECT_TYPE_SWITCH, switch_id);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("remove switch %s failed: %s",
                sai_serialize_object_id(switch_id).c_str(),
                sai_serialize_status(status).c_str());
    }

    status = sai->apiUninitialize();

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("sai_api_uninitialize failed: %s",
                sai_serialize_status(status).c_str());

        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
