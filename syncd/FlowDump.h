#pragma once

#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <string>
#include <zlib.h>

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

namespace syncd
{
    struct FlowDumpData
    {
        std::vector<nlohmann::json> json_lines;
    };

    using FlowDumpDataPtr = std::shared_ptr<FlowDumpData>;

    namespace FlowDumpSerializer
    {
        nlohmann::json serializeFlowEntryToJson(
           _In_ const sai_flow_bulk_get_session_event_data_t& event_data);

        std::string serializeAttributeValue(
           _In_ const sai_attribute_t& attr,
           _In_ const sai_attr_metadata_t* meta);
    }

    class FlowDumpWriter
    {
        public:

            static constexpr const char* DEFAULT_BASE_PATH = "/var/dump/flows/";
            static constexpr const char* FLOW_DUMP_FILE_PREFIX = "flow_dump_0x";
            static constexpr const char* FLOW_DUMP_FILE_SUFFIX = ".jsonl.gz";
            static constexpr size_t MAX_FILES = 2;

            static FlowDumpWriter& getInstance();

            bool writeFlowDumpData(
                    _In_ const FlowDumpDataPtr& data,
                    _In_ sai_object_id_t flow_bulk_session_vid);

            const std::string& getBasePath() const;

            void setBasePath(
                    _In_ const std::string& base_path);

        private:

            FlowDumpWriter();
            ~FlowDumpWriter();

            FlowDumpWriter(const FlowDumpWriter&) = delete;
            FlowDumpWriter& operator=(const FlowDumpWriter&) = delete;

            std::string getFilePath(
                    _In_ sai_object_id_t flow_bulk_session_vid) const;

            bool openFile(
                    _In_ const std::string& file_path,
                    _Out_ gzFile& gz_file);

            bool writeData(
                    _In_ gzFile gz_file,
                    _In_ const std::string& data);

            void flush(
                    _In_ gzFile gz_file);

            void closeFile(
                    _In_ gzFile gz_file,
                    _In_ const std::string& file_path);

            bool fileExists(
                    _In_ const std::string& file_path) const;

            void rotateLogFiles(
                    _In_ const std::string& target_file_path);

        private:

            std::string m_base_path;
    };
}
