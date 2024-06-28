#pragma once
#include "swss/table.h"
#include "meta/sai_serialize.h"
#include <nlohmann/json.hpp>

namespace syncd
{
    class SaiDump
    {
        public:
            SaiDump();
            ~SaiDump();
            void handleCmdLine(int argc, char **argv);
            void dumpFromRedisDb(int argc, char **argv);
            void printUsage();
            sai_status_t dumpFromRedisRdbJson();
            void traverseJson(const nlohmann::json & jsn);
            void dumpGraphFun(const swss::TableDump& td);
            void printAttributes(size_t indent, const swss::TableMap& map);
            void dumpGraphTable(const swss::TableDump &dump);
            std::string getRdbJsonFile();
            uint64_t getRdbJSonSizeLimit();
            bool getDumpTempView();
            bool getDumpGraph();
        private:
            std::string rdbJsonFile;
            uint64_t rdbJSonSizeLimit;
            bool dumpTempView;
            bool dumpGraph;
        private:
            std::map<sai_object_id_t, const swss::TableMap*> mOidMap;
        private:
            size_t getMaxAttrLen(const swss::TableMap& map);
            std::string padString(std::string s, size_t pad);
    };
}
