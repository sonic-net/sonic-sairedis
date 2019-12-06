#include <inttypes.h>
#include <string>
#include <set>
#include <sstream>

extern "C" {
#include <sai.h>
}

#include "swss/table.h"
#include "meta/sai_serialize.h"
#include "sairediscommon.h"

#include <getopt.h>

using namespace swss;

struct CmdOptions
{
    bool skipAttributes;
    bool dumpTempView;
    bool dumpGraph;
};

CmdOptions g_cmdOptions;
std::map<sai_object_id_t, const TableMap*> g_oid_map;

void printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: saidump [-t] [-g] [-h]" << std::endl;
    std::cout << "    -t --tempView:" << std::endl;
    std::cout << "        Dump temp view" << std::endl;
    std::cout << "    -g --dumpGraph:" << std::endl;
    std::cout << "        Dump current graph" << std::endl;
    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}

CmdOptions handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    CmdOptions options;

    options.dumpTempView = false;
    options.dumpGraph = false;

    const char* const optstring = "gth";

    while(true)
    {
        static struct option long_options[] =
        {
            { "dumpGraph",      no_argument,       0, 'g' },
            { "tempView",       no_argument,       0, 't' },
            { "help",           no_argument,       0, 'h' },
            { 0,                0,                 0,  0  }
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'g':
                SWSS_LOG_NOTICE("Dumping graph");
                options.dumpGraph = true;
                break;

            case 't':
                SWSS_LOG_NOTICE("Dumping temp view");
                options.dumpTempView = true;
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

    return options;
}

size_t get_max_attr_len(const TableMap& map)
{
    SWSS_LOG_ENTER();

    size_t max = 0;

    for (const auto&field: map)
    {
        max = std::max(max, field.first.length());
    }

    return max;
}

std::string pad_string(std::string s, size_t pad)
{
    SWSS_LOG_ENTER();

    size_t len = s.length();

    if (len < pad)
    {
        s.insert(len, pad - len, ' ');
    }

    return s;
}

const TableMap* get_table_map(sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    auto it = g_oid_map.find(object_id);

    if (it == g_oid_map.end())
    {
        SWSS_LOG_THROW("unable to find oid 0x%" PRIx64 " in oid map", object_id);
    }

    return it->second;
}

void print_attributes(size_t indent, const TableMap& map)
{
    SWSS_LOG_ENTER();

    size_t max_len = get_max_attr_len(map);

    std::string str_indent = pad_string("", indent);

    for (const auto&field: map)
    {
        const sai_attr_metadata_t *meta;
        sai_deserialize_attr_id(field.first, &meta);

        std::stringstream ss;

        ss << str_indent << pad_string(field.first, max_len) << " : ";

        ss << field.second;

        std::cout << ss.str() << std::endl;
    }
}

// colors are in HSV
#define GV_ARROW_COLOR  "0.650 0.700 0.700"
#define GV_ROOT_COLOR   "0.650 0.200 1.000"
#define GV_NODE_COLOR   "0.650 0.500 1.000"

void dumpGraph(const TableDump& td)
{
    SWSS_LOG_ENTER();

    std::map<sai_object_id_t, const sai_object_type_info_t*> oidtypemap;
    std::map<sai_object_type_t,const sai_object_type_info_t*> typemap;

    std::cout << "digraph \"SAI Object Dependency Graph\" {" << std::endl;
    std::cout << "size = \"30,12\"; ratio = fill;" << std::endl;
    std::cout << "node [ style = filled ];" << std::endl;

    // build object type map first

    std::set<sai_object_type_t> definedtypes;

    std::map<sai_object_type_t, int> usagemap;

    for (const auto& key: td)
    {
        sai_object_meta_key_t meta_key;
        sai_deserialize_object_meta_key(key.first, meta_key);

        auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

        typemap[info->objecttype] = info;

        if (!info->isnonobjectid)
            oidtypemap[meta_key.objectkey.key.object_id] = info;

        if (definedtypes.find(meta_key.objecttype) != definedtypes.end())
            continue;

        definedtypes.insert(meta_key.objecttype);
    }

    std::set<std::string> definedlinks;

    std::set<sai_object_type_t> ref;
    std::set<sai_object_type_t> attrref;

#define SAI_OBJECT_TYPE_PREFIX_LEN (sizeof("SAI_OBJECT_TYPE_") - 1)

    for (const auto& key: td)
    {
        sai_object_meta_key_t meta_key;
        sai_deserialize_object_meta_key(key.first, meta_key);

        auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

        // process non object id objects if any
        for (size_t j = 0; j < info->structmemberscount; ++j)
        {
            const sai_struct_member_info_t *m = info->structmembers[j];

            if (m->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                sai_object_id_t member_oid = m->getoid(&meta_key);

                auto member_info = oidtypemap.at(member_oid);

                if (member_info->objecttype == SAI_OBJECT_TYPE_SWITCH)
                {
                    // skip link of SWITCH to non object id object types, since
                    // all of them contain switch_id
                    continue;
                }

                std::stringstream ss;

                ss << std::string(member_info->objecttypename + SAI_OBJECT_TYPE_PREFIX_LEN) << " -> "
                << std::string(info->objecttypename + SAI_OBJECT_TYPE_PREFIX_LEN)
                << "[ color=\"" << GV_ARROW_COLOR << "\", style = dashed, penwidth = 2 ]";

                std::string link = ss.str();

                if (definedlinks.find(link) != definedlinks.end())
                    continue;

                definedlinks.insert(link);

                std::cout << link << std::endl;
            }
        }

        // process attributes for this object

        for (const auto&field: key.second)
        {
            const sai_attr_metadata_t *meta;
            sai_deserialize_attr_id(field.first, &meta);

            if (!meta->isoidattribute || meta->isreadonly)
            {
                // skip non oid attributes and read only attributes
                continue;
            }

            sai_attribute_t attr;

            sai_deserialize_attr_value(field.second, *meta, attr, false);

            sai_object_list_t list = { 0, NULL };

            switch (meta->attrvaluetype)
            {
                case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                    list.count = 1;
                    list.list = &attr.value.oid;
                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                    if (attr.value.aclfield.enable)
                    {
                        list.count = 1;
                        list.list = &attr.value.aclfield.data.oid;
                    }
                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                    if (attr.value.aclaction.enable)
                    {
                        list.count = 1;
                        list.list = &attr.value.aclaction.parameter.oid;
                    }
                    break;

                case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                    list = attr.value.objlist;
                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                    if (attr.value.aclfield.enable)
                        list = attr.value.aclfield.data.objlist;
                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                    if (attr.value.aclaction.enable)
                        list = attr.value.aclaction.parameter.objlist;
                    break;

                default:
                    SWSS_LOG_THROW("attr value type: %d is not supported, FIXME", meta->attrvaluetype);
            }

            for (uint32_t i = 0; i < list.count; ++i)
            {
                sai_object_id_t oid = list.list[i];

                if (oid == SAI_NULL_OBJECT_ID)
                    continue;

                // this object type is not root, can be in the middle or leaf
                ref.insert(info->objecttype);

                auto attr_oid_info = oidtypemap.at(oid);

                std::stringstream ss;

                attrref.insert(attr_oid_info->objecttype);

                ss << std::string(attr_oid_info->objecttypename + SAI_OBJECT_TYPE_PREFIX_LEN) << " -> "
                << std::string(info->objecttypename + SAI_OBJECT_TYPE_PREFIX_LEN)
                << "[ color = \"" << GV_ARROW_COLOR << "\" ]";

                std::string link = ss.str();

                if (definedlinks.find(link) != definedlinks.end())
                    continue;

                definedlinks.insert(link);

                std::cout << link << std::endl;
            }

            sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
        }
    }

    for (auto t: typemap)
    {
        auto ot = t.first;
        auto info = t.second;

        auto name = std::string(info->objecttypename + SAI_OBJECT_TYPE_PREFIX_LEN);

        if (info->isnonobjectid)
        {
            /* non object id leafs */

            std::cout << name << " [ color = plum, shape = rect ];\n";
            continue;
        }

        if (ref.find(ot) != ref.end() && attrref.find(ot) != attrref.end())
        {
            /* middle nodes */

            std::cout << name << " [ color =\"" << GV_NODE_COLOR << "\" ];\n";
            continue;
        }

        if (ref.find(ot) != ref.end() && attrref.find(ot) == attrref.end())
        {
            /* leafs */

            std::cout << name << " [ color = palegreen, shape = rect ];\n";
            continue;
        }

        if (ref.find(ot) == ref.end() && attrref.find(ot) != attrref.end())
        {
            /* roots */

            std::cout << name << " [ color = \"" << GV_ROOT_COLOR << "\" ];\n";
            continue;
        }

        /* objects which are there but not referenced nowhere for example STP */

        std::cout << name << " [ color = \"" << GV_ROOT_COLOR << "\", shape = rect ];\n";
    }

    std::cout << "SWITCH -> PORT [ dir = none, color = red, peripheries = 2, penwidth = 2, style = dashed ];" << std::endl;
    std::cout << "SWITCH [ color = orange, fillcolor = orange, shape = parallelogram, peripheries = 2 ];" << std::endl;
    std::cout << "PORT [ color = gold, shape = diamond, peripheries = 2 ];" << std::endl;
    std::cout << "}" << std::endl;
}

int main(int argc, char ** argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    meta_init_db();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    g_cmdOptions = handleCmdLine(argc, argv);

    swss::DBConnector db(ASIC_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);

    std::string table = ASIC_STATE_TABLE;

    if (g_cmdOptions.dumpTempView)
    {
        table = TEMP_PREFIX + table;
    }

    swss::Table t(&db, table);

    TableDump dump;

    t.dump(dump);

    for (const auto&key: dump)
    {
        auto start = key.first.find_first_of(":");
        auto str_object_type = key.first.substr(0, start);
        auto str_object_id  = key.first.substr(start + 1);

        sai_object_type_t object_type;
        sai_deserialize_object_type(str_object_type, object_type);

        auto info = sai_metadata_get_object_type_info(object_type);

        if (!info->isnonobjectid)
        {
            sai_object_id_t object_id;
            sai_deserialize_object_id(str_object_id, object_id);

            g_oid_map[object_id] = &key.second;
        }
    }

    if (g_cmdOptions.dumpGraph)
    {
        dumpGraph(dump);

        return EXIT_SUCCESS;
    }

    for (const auto&key: dump)
    {
        auto start = key.first.find_first_of(":");
        auto str_object_type = key.first.substr(0, start);
        auto str_object_id  = key.first.substr(start + 1);

        std::cout << str_object_type << " " << str_object_id << " " << std::endl;

        size_t indent = 4;

        print_attributes(indent, key.second);

        std::cout << std::endl;
    }

    return EXIT_SUCCESS;
}
