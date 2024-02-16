#pragma once
#include <inttypes.h>

// Default value: 100MB
constexpr int64_t RDB_JSON_MAX_SIZE = 1024 * 1024 * 100;

struct CmdOptions
{
    bool skipAttributes;
    bool dumpTempView;
    bool dumpGraph;
    std::string rdbJsonFile;
    uint64_t rdbJSonSizeLimit;
};

void printUsage();
CmdOptions handleCmdLine(int argc, char **argv);
sai_status_t dumpFromRedisRdbJson(const std::string file_name);
sai_status_t preProcessFile(const std::string file_name);
