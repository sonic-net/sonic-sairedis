#include "FlowDump.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include "swss/logger.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <exception>
#include <stdexcept>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace syncd
{
    constexpr const char* FlowDumpWriter::DEFAULT_BASE_PATH;

    std::string FlowDumpSerializer::serializeAttributeValue(
            _In_ const sai_attribute_t& attr,
            _In_ const sai_attr_metadata_t* meta)
    {
        SWSS_LOG_ENTER();

        if (meta == nullptr)
        {
            return std::string();
        }

        // SAI_FLOW_ENTRY_ATTR_DASH_FLOW_ACTION is a bit mask not a single enum value.
        if (attr.id == SAI_FLOW_ENTRY_ATTR_DASH_FLOW_ACTION)
        {
            return std::to_string(attr.value.s32);
        }

        try
        {
            return sai_serialize_attr_value(*meta, attr, false);
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("Exception in serializeAttributeValue for attr %s: %s",
                          meta != nullptr ? meta->attridname : "unknown", e.what());
            return std::string();
        }
        catch (...)
        {
            SWSS_LOG_ERROR("Unknown exception in serializeAttributeValue for attr %s",
                          meta != nullptr ? meta->attridname : "unknown");
            return std::string();
        }
    }

    nlohmann::json FlowDumpSerializer::serializeFlowEntryToJson(
            _In_ const sai_flow_bulk_get_session_event_data_t& event_data)
    {
        SWSS_LOG_ENTER();

        nlohmann::json json_line;

        json_line["epoch"] = std::time(nullptr);

        const sai_flow_entry_t& flow_entry = event_data.flow_entry;
        json_line["em"] = sai_serialize_mac(flow_entry.eni_mac);
        json_line["vn"] = flow_entry.vnet_id;
        json_line["pr"] = flow_entry.ip_proto;
        json_line["si"] = sai_serialize_ip_address(flow_entry.src_ip);
        json_line["di"] = sai_serialize_ip_address(flow_entry.dst_ip);
        json_line["sp"] = flow_entry.src_port;
        json_line["dp"] = flow_entry.dst_port;

        if (event_data.attr != nullptr && event_data.attr_count > 0)
        {
            for (uint32_t j = 0; j < event_data.attr_count; ++j)
            {
                const sai_attribute_t& attr = event_data.attr[j];
                auto meta = sai_metadata_get_attr_metadata(static_cast<sai_object_type_t>(SAI_OBJECT_TYPE_FLOW_ENTRY), attr.id);
                if (meta != nullptr)
                {
                    json_line[std::to_string(attr.id)] = FlowDumpSerializer::serializeAttributeValue(attr, meta);
                }
            }
        }

        return json_line;
    }

    FlowDumpWriter::FlowDumpWriter() :
        m_base_path(DEFAULT_BASE_PATH)
    {
        SWSS_LOG_ENTER();
    }

    FlowDumpWriter& FlowDumpWriter::getInstance()
    {
        SWSS_LOG_ENTER();
        static FlowDumpWriter instance;
        return instance;
    }

    FlowDumpWriter::~FlowDumpWriter()
    {
        SWSS_LOG_ENTER();
    }

    std::string FlowDumpWriter::getFilePath(
            _In_ sai_object_id_t flow_bulk_session_vid) const
    {
        SWSS_LOG_ENTER();

        std::ostringstream oss;
        oss << m_base_path << FLOW_DUMP_FILE_PREFIX
            << std::hex << std::setfill('0') << std::setw(16)
            << flow_bulk_session_vid << FLOW_DUMP_FILE_SUFFIX;

        return oss.str();
    }

    bool FlowDumpWriter::openFile(
            _In_ const std::string& file_path,
            _Out_ gzFile& gz_file)
    {
        SWSS_LOG_ENTER();

        // Extract directory path from file path
        std::string dir_path = file_path;
        size_t last_slash = dir_path.find_last_of('/');
        if (last_slash != std::string::npos)
        {
            dir_path = dir_path.substr(0, last_slash);
        }
        else
        {
            dir_path = ".";
        }
        if (!dir_path.empty() && dir_path != ".")
        {
            struct stat st;
            if (stat(dir_path.c_str(), &st) != 0)
            {
                SWSS_LOG_ERROR("Failed to write flows. Directory %s does not exist", dir_path.c_str());
                return false;
            }
            else if (!S_ISDIR(st.st_mode))
            {
                SWSS_LOG_ERROR("Failed to write flows. Path %s exists but is not a directory", dir_path.c_str());
                return false;
            }
        }

        gz_file = gzopen(file_path.c_str(), "ab");
        if (gz_file == nullptr)
        {
            int err = errno;
            SWSS_LOG_ERROR("Failed to open file %s for writing: errno=%d(%s)",
                           file_path.c_str(),
                           err,
                           std::strerror(err));
            return false;
        }

        SWSS_LOG_DEBUG("Opened flow dump file: %s", file_path.c_str());

        return true;
    }

    bool FlowDumpWriter::writeData(
            _In_ gzFile gz_file,
            _In_ const std::string& data)
    {
        SWSS_LOG_ENTER();

        if (gz_file == nullptr)
        {
            SWSS_LOG_ERROR("File not open");
            return false;
        }

        if (data.empty())
        {
            return true;
        }

        size_t data_len = data.length();
        if (data_len > UINT32_MAX)
        {
            SWSS_LOG_WARN("Data length %zu exceeds UINT32_MAX, writing first UINT32_MAX bytes only",
                          data_len);
            data_len = UINT32_MAX;
        }

        unsigned int len = static_cast<unsigned int>(data_len);
        int written = gzwrite(gz_file, data.c_str(), len);

        if (written != static_cast<int>(len))
        {
            SWSS_LOG_ERROR("Failed to write data to file: wrote %d of %u bytes", written, len);
            return false;
        }

        return true;
    }

    void FlowDumpWriter::flush(
            _In_ gzFile gz_file)
    {
        SWSS_LOG_ENTER();

        if (gz_file != nullptr)
        {
            gzflush(gz_file, Z_SYNC_FLUSH);
        }
    }

    bool FlowDumpWriter::writeFlowDumpData(
            _In_ const FlowDumpDataPtr& data,
            _In_ sai_object_id_t flow_bulk_session_vid)
    {
        SWSS_LOG_ENTER();

        if (data == nullptr)
        {
            SWSS_LOG_ERROR("FlowDumpData is null");
            return false;
        }

        std::string file_path = getFilePath(flow_bulk_session_vid);

        if (!fileExists(file_path))
        {
            rotateLogFiles(file_path);
        }

        std::ostringstream oss;
        try
        {
            for (const auto& json_line : data->json_lines)
            {
                oss << json_line.dump() << "\n";
            }
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("Failed to serialize JSON lines: %s", e.what());
            return false;
        }

        gzFile gz_file = nullptr;
        if (!openFile(file_path, gz_file))
        {
            return false;
        }

        bool success = writeData(gz_file, oss.str());
        if (!success)
        {
            SWSS_LOG_ERROR("Failed to write flow dump data");
        }

        flush(gz_file);
        closeFile(gz_file, file_path);

        return success;
    }

    void FlowDumpWriter::closeFile(
            _In_ gzFile gz_file,
            _In_ const std::string& file_path)
    {
        SWSS_LOG_ENTER();

        if (gz_file != nullptr)
        {
            gzclose(gz_file);
            SWSS_LOG_DEBUG("Closed flow dump file: %s", file_path.c_str());
        }
    }

    const std::string& FlowDumpWriter::getBasePath() const
    {
        SWSS_LOG_ENTER();

        return m_base_path;
    }

    void FlowDumpWriter::setBasePath(
            _In_ const std::string& base_path)
    {
        SWSS_LOG_ENTER();

        m_base_path = base_path;
        SWSS_LOG_DEBUG("Set flow dump base path to: %s", m_base_path.c_str());
    }

    bool FlowDumpWriter::fileExists(
            _In_ const std::string& file_path) const
    {
        SWSS_LOG_ENTER();

        struct stat st;
        if (stat(file_path.c_str(), &st) != 0)
        {
            return false;
        }
        return S_ISREG(st.st_mode);
    }

    void FlowDumpWriter::rotateLogFiles(
            _In_ const std::string& target_file_path)
    {
        SWSS_LOG_ENTER();

        std::vector<std::pair<std::string, std::time_t>> files;

        // Check if base directory exists and is a directory
        struct stat st;
        if (stat(m_base_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
        {
            return;
        }

        // Extract target filename for comparison
        size_t last_slash = target_file_path.find_last_of('/');
        std::string target_filename = (last_slash != std::string::npos) ?
                                      target_file_path.substr(last_slash + 1) :
                                      target_file_path;

        DIR* dir = opendir(m_base_path.c_str());
        if (dir == nullptr)
        {
            SWSS_LOG_WARN("Failed to open directory %s: %s", m_base_path.c_str(), strerror(errno));
            return;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            std::string filename = entry->d_name;
            std::string full_path = m_base_path;
            if (full_path.back() != '/')
            {
                full_path += '/';
            }
            full_path += filename;

            // Check if it's a regular file
            if (stat(full_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            {
                continue;
            }

            // Check if it matches flow dump file pattern
            if (filename.find(FLOW_DUMP_FILE_PREFIX) == 0 && filename.find(FLOW_DUMP_FILE_SUFFIX) != std::string::npos)
            {
                // Skip the target file
                if (filename == target_filename)
                {
                    continue;
                }

                files.push_back(std::make_pair(full_path, st.st_mtime));
            }
        }
        closedir(dir);

        if (files.size() < MAX_FILES)
        {
            return;
        }

        std::sort(files.begin(), files.end(),
                  [](const std::pair<std::string, std::time_t>& a,
                     const std::pair<std::string, std::time_t>& b) {
                      return a.second < b.second;
                  });

        size_t files_to_delete = files.size() + 1 - MAX_FILES;

        for (size_t i = 0; i < files_to_delete && i < files.size(); ++i)
        {
            const std::string& file_to_delete = files[i].first;
            if (unlink(file_to_delete.c_str()) == 0)
            {
                SWSS_LOG_NOTICE("Deleted oldest flow dump file: %s", file_to_delete.c_str());
            }
            else
            {
                SWSS_LOG_ERROR("Failed to delete flow dump file %s: %s",
                               file_to_delete.c_str(), strerror(errno));
            }
        }
    }
}
