#include "sai_redis.h"
#include <string.h>

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

std::ofstream recording;
std::mutex g_recordMutex;

void recordLine(std::string s)
{
    std::lock_guard<std::mutex> lock(g_recordMutex);

    SWSS_LOG_ENTER();

    if (recording.is_open())
    {
        recording << getTimestamp() << "|" << s << std::endl;
    }
}

std::string recfile = "dummy.rec";

void startRecording()
{
    SWSS_LOG_ENTER();

    recfile = "sairedis." + getTimestamp() + ".rec";

    recording.open(recfile);

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
