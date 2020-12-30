#include "sai_redis.h"
#include <string.h>
#include <unistd.h>

std::string logOutputDir = ".";
std::string logOutputFile = "sairedis.rec";

std::string getTimestamp()
{
    SWSS_LOG_ENTER();

    char buffer[64];
    struct timeval tv;

    gettimeofday(&tv, NULL);

    size_t size = strftime(buffer, 32 ,"%Y-%m-%d.%T.", localtime(&tv.tv_sec));

    snprintf(&buffer[size], 32, "%06ld", tv.tv_usec);

    return std::string(buffer);
}

// recording needs to be enabled explicitly
volatile bool g_record = false;
volatile bool g_logrotate = false;

std::ofstream recording;
std::mutex g_recordMutex;

std::string recfile = "dummy.rec";

void logfileReopen()
{
    SWSS_LOG_ENTER();

    recording.close();

    /*
     * On log rotate we will use the same file name, we are assuming that
     * logrotate daemon move filename to filename.1 and we will create new
     * empty file here.
     */

    recording.open(recfile, std::ofstream::out | std::ofstream::app);

    if (!recording.is_open())
    {
        SWSS_LOG_ERROR("failed to open recording file %s: %s", recfile.c_str(), strerror(errno));
        return;
    }
}

void recordLine(std::string s)
{
    std::lock_guard<std::mutex> lock(g_recordMutex);

    SWSS_LOG_ENTER();

    if (recording.is_open())
    {
        recording << getTimestamp() << "|" << s << std::endl;
    }

    if (g_logrotate)
    {
        g_logrotate = false;

        logfileReopen();

        /* double check since reopen could fail */

        if (recording.is_open())
        {
            recording << getTimestamp() << "|" << "#|logrotate on: " << recfile << std::endl;
        }
    }
}

void startRecording()
{
    SWSS_LOG_ENTER();

    recfile = logOutputDir + "/" + logOutputFile ;

    recording.open(recfile, std::ofstream::out | std::ofstream::app);

    if (!recording.is_open())
    {
        SWSS_LOG_ERROR("failed to open recording file %s: %s", recfile.c_str(), strerror(errno));
        return;
    }

    recordLine("#|recording on: " + recfile);

    SWSS_LOG_NOTICE("started recording: %s", recfile.c_str());
}

void stopRecording()
{
    SWSS_LOG_ENTER();

    if (recording.is_open())
    {
        recording.close();

        SWSS_LOG_NOTICE("stopped recording: %s", recfile.c_str());
    }
}

void setRecording(bool record)
{
    SWSS_LOG_ENTER();

    g_record = record;

    stopRecording();

    if (record)
    {
        startRecording();
    }
}

std::string joinFieldValues(
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    for (size_t i = 0; i < values.size(); ++i)
    {
        const std::string &str_attr_id = fvField(values[i]);
        const std::string &str_attr_value = fvValue(values[i]);

        if(i != 0)
        {
            ss << "|";
        }

        ss << str_attr_id << "=" << str_attr_value;
    }

    return ss.str();
}

sai_status_t setRecordingOutputDir(
        _In_ const sai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    if (attr.value.s8list.count == 0)
    {
        logOutputDir = ".";
        return SAI_STATUS_SUCCESS;
    }

    if (attr.value.s8list.list == NULL)
    {
        SWSS_LOG_ERROR("list pointer is NULL");
        return SAI_STATUS_FAILURE;
    }

    size_t len = strnlen((const char *)attr.value.s8list.list, attr.value.s8list.count);

    if (len != (size_t)attr.value.s8list.count)
    {
        SWSS_LOG_ERROR("count (%u) is different than strnlen (%zu)", attr.value.s8list.count, len);
        return SAI_STATUS_FAILURE;
    }

    std::string dir((const char*)attr.value.s8list.list, len);

    int result = access(dir.c_str(), W_OK);

    if (result != 0)
    {
        SWSS_LOG_ERROR("can't access dir '%s' for writing", dir.c_str());
        return SAI_STATUS_FAILURE;
    }

    logOutputDir = dir;

    /* perform auto log rotate when log directory gets changed */

    g_logrotate = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t setRecordingOutputFile(
    _In_ const sai_attribute_t &attr)
{
     SWSS_LOG_ENTER();

    if (attr.value.s8list.count == 0)
    {
        SWSS_LOG_NOTICE("setting recording filename to default filename: %s", logOutputFile.c_str());

        return true;
    }

    if (attr.value.s8list.list == NULL)
    {
        SWSS_LOG_ERROR("list pointer is NULL");

        return false;
    }

    size_t len = strnlen((const char *)attr.value.s8list.list, attr.value.s8list.count);

    if (len != (size_t)attr.value.s8list.count)
    {
        SWSS_LOG_ERROR("count (%u) is different than strnlen (%zu)", attr.value.s8list.count, len);

        return false;
    }

    std::string filename((const char*)attr.value.s8list.list, len);

    /// Stop the recording with old file before updating the filename
    stopRecording();

    logOutputFile = filename;

    SWSS_LOG_NOTICE("setting recording filename : %s", logOutputFile.c_str());

    /// Start recording with new file
    startRecording();
    return SAI_STATUS_SUCCESS;

}