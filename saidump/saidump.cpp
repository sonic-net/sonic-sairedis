#include <string>

extern "C" {
#include <sai.h>
}

#include "swss/table.h"
#include "meta/saiserialize.h"
#include "sairedis.h"

#include <getopt.h>

using namespace swss;

struct CmdOptions
{
    bool skipAttributes;
    bool dumpTempView;
};

CmdOptions g_cmdOptions;
std::map<sai_object_id_t, const TableMap*> g_oid_map;

void printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: saidump [-t] [-h]" << std::endl;
    std::cout << "    -t --tempView:" << std::endl;
    std::cout << "        Dump temp view" << std::endl;
    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}

CmdOptions handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    CmdOptions options;

    options.dumpTempView = false;

    const char* const optstring = "th";

    while(true)
    {
        static struct option long_options[] =
        {
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
        SWSS_LOG_THROW("unable to find oid 0x%lx in oid map", object_id);
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
}
