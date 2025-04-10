#include "Recorder.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"
#include "meta/Globals.h"
#include "meta/SaiInterface.h"

#include <unistd.h>
#include <inttypes.h>

#include <cstring>
#include <vector>
#include <fstream>

using namespace sairedis;
using namespace saimeta;

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const sai_stat_id_t *counter_id_list);

#define MUTEX() std::lock_guard<std::mutex> _lock(m_mutex)
#define DEFAULT_RECORDING_FILE_NAME "sairedis.rec"
Recorder::Recorder()
{
    SWSS_LOG_ENTER();

    m_recordingFileName = DEFAULT_RECORDING_FILE_NAME;

    m_recordingOutputDirectory = ".";

    m_performLogRotate = false;

    m_enabled = false;

    m_recordStats = true;
}

Recorder::~Recorder()
{
    SWSS_LOG_ENTER();

    stopRecording();
}

bool Recorder::setRecordingOutputDirectory(
        _In_ const sai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    if (attr.value.s8list.count == 0)
    {
        m_recordingOutputDirectory = ".";

        SWSS_LOG_NOTICE("setting recording directory to: %s", m_recordingOutputDirectory.c_str());

        requestLogRotate();

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

    std::string dir((const char*)attr.value.s8list.list, len);

    int result = access(dir.c_str(), W_OK);

    if (result != 0)
    {
        SWSS_LOG_ERROR("can't access dir '%s' for writing", dir.c_str());

        return false;
    }

    m_recordingOutputDirectory = dir;

    // perform log rotate when log directory gets changed

    requestLogRotate();

    return true;
}

bool Recorder::setRecordingFilename(
    _In_ const sai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    if (attr.value.s8list.count == 0)
    {
        m_recordingFileName = DEFAULT_RECORDING_FILE_NAME;

        SWSS_LOG_NOTICE("setting recording filename to default filename: %s", m_recordingFileName.c_str());

        requestLogRotate();

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
    if (m_enabled)
    {
        stopRecording();
    }

    m_recordingFileName = filename;

    SWSS_LOG_NOTICE("setting recording filename : %s", m_recordingFileName.c_str());

    /// Start recording with new file
    if (m_enabled)
    {
         startRecording();
    }
    return true;
}

void Recorder::enableRecording(
        _In_ bool enabled)
{
    SWSS_LOG_ENTER();

    m_enabled = enabled;

    stopRecording();

    if (enabled)
    {
        startRecording();
    }
}

void Recorder::recordLine(
        _In_ const std::string& line)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (!m_enabled)
    {
        return;
    }

    if (m_ofstream.is_open())
    {
        m_ofstream << getTimestamp() << "|" << line << std::endl;
    }
}

void Recorder::requestLogRotate()
{
    SWSS_LOG_ENTER();

    recordingFileReopen();

    /* double check since reopen could fail */

    recordLine("#|logrotate on: " + m_recordingFile);
}

void Recorder::recordingFileReopen()
{
    MUTEX();

    SWSS_LOG_ENTER();

    m_ofstream.close();

    /*
     * On log rotate we will use the same file name, we are assuming that
     * logrotate daemon move filename to filename.1 and we will create new
     * empty file here.
     */

    m_recordingFile = m_recordingOutputDirectory + "/" + m_recordingFileName;

    m_ofstream.open(m_recordingFile, std::ofstream::out | std::ofstream::app);

    if (!m_ofstream.is_open())
    {
        SWSS_LOG_ERROR("failed to open recording file %s: %s", m_recordingFile.c_str(), strerror(errno));
        return;
    }
}

void Recorder::startRecording()
{
    SWSS_LOG_ENTER();

    m_recordingFile = m_recordingOutputDirectory + "/" + m_recordingFileName;

    {
        MUTEX();
        m_ofstream.open(m_recordingFile, std::ofstream::out | std::ofstream::app);

        if (!m_ofstream.is_open())
        {
            SWSS_LOG_ERROR("failed to open recording file %s: %s", m_recordingFile.c_str(), strerror(errno));
            return;
        }
    }

    recordLine("#|recording on: " + m_recordingFile);

    SWSS_LOG_NOTICE("started recording: %s", m_recordingFileName.c_str());
}

void Recorder::stopRecording()
{
    MUTEX();

    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("stopped recording");

    if (m_ofstream.is_open())
    {
        m_ofstream.close();

        SWSS_LOG_NOTICE("closed recording file: %s", m_recordingFileName.c_str());
    }
}

std::string Recorder::getTimestamp()
{
    SWSS_LOG_ENTER();

    char buffer[64];
    struct timeval tv;

    gettimeofday(&tv, NULL);

    struct tm now;
    localtime_r(&tv.tv_sec, &now);

    size_t size = strftime(buffer, 32, "%Y-%m-%d.%T.", &now);

    snprintf(&buffer[size], 32, "%06ld", tv.tv_usec);

    return std::string(buffer);
}

// SAI APIs record functions

void Recorder::recordFlushFdbEntries(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_FLUSH,
            attrCount,
            attrList,
            false);

    std::string serializedObjectType = sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH);

    // NOTE ! we actually give switch ID since FLUSH is not real object
    std::string key = serializedObjectType + ":" + sai_serialize_object_id(switchId);

    SWSS_LOG_NOTICE("flush key: %s, fields: %lu", key.c_str(), entry.size());

    recordFlushFdbEntries(key, entry);
}

void Recorder::recordFlushFdbEntries(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("f|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordFlushFdbEntriesResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    recordLine("F|" + sai_serialize_status(status));
}

void Recorder::recordQueryAttributeCapability(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("q|attribute_capability|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordQueryAttributeCapabilityResponse(
        _In_ sai_status_t status,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("Q|attribute_capability|" + sai_serialize_status(status) + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordQueryAttributeEnumValuesCapability(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("q|attribute_enum_values_capability|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordQueryAttributeEnumValuesCapabilityResponse(
        _In_ sai_status_t status,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("Q|attribute_enum_values_capability|" + sai_serialize_status(status) + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordObjectTypeGetAvailability(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("q|object_type_get_availability|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordObjectTypeGetAvailabilityResponse(
        _In_ sai_status_t status,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("Q|object_type_get_availability|" + sai_serialize_status(status) + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordQueryStatsCapability(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("q|stats_capability|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordQueryStatsCapabilityResponse(
        _In_ sai_status_t status,
        _In_ const std::string& arguments)
{
    SWSS_LOG_ENTER();

    recordLine("Q|stats_capability|" + sai_serialize_status(status) + "|" + arguments);
}

void Recorder::recordQueryStatsStCapability(
    _In_ const std::string &key,
    _In_ const std::vector<swss::FieldValueTuple> &arguments)
{
    SWSS_LOG_ENTER();

    recordLine("q|stats_st_capability|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordQueryStatsStCapabilityResponse(
    _In_ sai_status_t status,
    _In_ const std::string &arguments)
{
    SWSS_LOG_ENTER();

    recordLine("Q|stats_st_capability|" + sai_serialize_status(status) + "|" + arguments);
}

void Recorder::recordNotifySyncd(
        _In_ const std::string& key)
{
    SWSS_LOG_ENTER();

    // lower case 'a' stands for notify syncd request

    recordLine("a|" + key);
}

void Recorder::recordNotifySyncdResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // capital 'A' stands for notify syncd response

    recordLine("A|" + sai_serialize_status(status));
}

void Recorder::recordGenericCreate(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    // lower case 'c' stands for create api

    recordLine("c|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordGenericCreateResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordGenericCreateResponse(
        _In_ sai_status_t status,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordBulkGenericCreate(
        _In_ const std::string& objectType,
        _In_ const std::vector<swss::FieldValueTuple>& entriesWithStatus)
{
    SWSS_LOG_ENTER();

    std::string joined;

    for (const auto &e: entriesWithStatus)
    {
        // ||obj_id|attr=val|attr=val|status||obj_id|attr=val|attr=val|status

        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    // capital 'C' stands for bulk CREATE operation.

    recordLine("C|" + objectType + joined);
}

void Recorder::recordBulkGenericCreateResponse(
        _In_ sai_status_t status,
        _In_ uint32_t objectCount,
        _In_ const sai_status_t *objectStatuses)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordGenericRemove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto key = sai_serialize_object_type(objectType) + ":" + sai_serialize_object_id(objectId);

    // lower case 'r' stands for REMOVE api
    recordLine("r|" + key);
}

void Recorder::recordGenericRemove(
        _In_ const std::string& key)
{
    SWSS_LOG_ENTER();

    // lower case 'r' stands for REMOVE api
    recordLine("r|" + key);
}

void Recorder::recordGenericRemoveResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordBulkGenericRemove(
        _In_ const std::string& objectType,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    std::string joined;

    // TODO revisit

    for (const auto &e: arguments)
    {
        // ||obj_id||obj_id||...

        joined += "||" + fvField(e);
    }

    // capital 'R' stands for bulk REMOVE operation.

    recordLine("R|" + objectType + joined);
}

void Recorder::recordBulkGenericRemoveResponse(
        _In_ sai_status_t status,
        _In_ uint32_t objectCount,
        _In_ const sai_status_t *objectStatuses)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordGenericSet(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    recordSet(objectType, sai_serialize_object_id(objectId), attr);
}

void Recorder::recordGenericSet(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    // lower case 's' stands for SET api

    recordLine("s|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordGenericCounterPolling(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    // lower case 'p' stands for counter Polling

    recordLine("p|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordGenericSetResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordBulkGenericSet(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    std::string joined;

    for (const auto &e: arguments)
    {
        // ||obj_id|attr=val|status||obj_id|attr=val|status

        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    // capital 'S' stands for bulk SET operation.

    recordLine("S|" + key + joined);
}

void Recorder::recordBulkGenericSetResponse(
        _In_ sai_status_t status,
        _In_ uint32_t objectCount,
        _In_ const sai_status_t *objectStatuses)
{
    SWSS_LOG_ENTER();

    // TODO currently empty since used in async mode, but we should log this in
    // synchronous mode, and we could use "G" from GET api as response
}

void Recorder::recordGenericGet(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    recordGet(
            objectType,
            sai_serialize_object_id(objectId),
            attr_count,
            attr_list);
}

void Recorder::recordGenericGet(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    // lower case 'g' stands for GET api

    recordLine("g|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordGenericGetResponse(
        _In_ sai_status_t status,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    // capital 'G' stands for GET api response

    recordLine("G|" + sai_serialize_status(status) + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordBulkGenericGet(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    std::string joined;

    for (const auto &e: arguments)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    // capital 'B' stands for Bulk GET operation. Note: 'G' already used for get response.

    recordLine("B|" + key + joined);
}

void Recorder::recordBulkGenericGetResponse(
        _In_ sai_status_t status,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    std::string joined;

    for (const auto &e: arguments)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    recordLine("G|" + sai_serialize_status(status) + "|" + joined);
}

void Recorder::recordGenericGetStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    if (!m_recordStats)
        return;

    auto stats_enum = sai_metadata_get_object_type_info(object_type)->statenum;

    auto entry = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + sai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic get stats key: %s, fields: %lu", key.c_str(), entry.size());

    recordGenericGetStats(key, entry);
}

void Recorder::recordGenericGetStats(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    if (!m_recordStats)
        return;

    recordLine("q|get_stats|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordGenericGetStatsResponse(
        _In_ sai_status_t status,
        _In_ uint32_t count,
        _In_ const uint64_t *counters)
{
    SWSS_LOG_ENTER();

    if (!m_recordStats)
        return;

    std::string joined;

    for (uint32_t idx = 0; idx < count; idx ++)
    {
        joined += "|" + std::to_string(counters[idx]);
    }

    recordLine("Q|get_stats|" + sai_serialize_status(status) + joined);
}

void Recorder::recordGenericClearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    if (!m_recordStats)
        return;

    auto stats_enum = sai_metadata_get_object_type_info(object_type)->statenum;

    auto values = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    std::string str_object_type = sai_serialize_object_type(object_type);
    std::string key = str_object_type + ":" + sai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic clear stats key: %s, fields: %lu", key.c_str(), values.size());

    recordGenericClearStats(key, values);
}

void Recorder::recordGenericClearStats(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& arguments)
{
    SWSS_LOG_ENTER();

    if (!m_recordStats)
        return;

    recordLine("q|clear_stats|" + key + "|" + Globals::joinFieldValues(arguments));
}

void Recorder::recordGenericClearStatsResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    if (!m_recordStats)
        return;

    recordLine("Q|clear_stats|" + sai_serialize_status(status));
}

void Recorder::recordNotification(
        _In_ const std::string &name,
        _In_ const std::string &serializedNotification,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    recordLine("n|" + name + "|" + serializedNotification + "|" + Globals::joinFieldValues(values));
}

void Recorder::recordRemove(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto key = sai_serialize_object_type(objectType) + ":" + serializedObjectId;

    recordGenericRemove(key);
}

void Recorder::recordGenericCreate(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId, // already allocated
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    recordCreate(
            objectType,
            sai_serialize_object_id(objectId),
            attr_count,
            attr_list);
}

void Recorder::recordCreate(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            objectType,
            attr_count,
            attr_list,
            false);

    if (entry.size() == 0)
    {
        // make sure that we put object into db
        // even if there are no attributes set
        swss::FieldValueTuple null("NULL", "NULL");

        entry.push_back(null);
    }

    std::string serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic create key: %s, fields: %" PRIu64, key.c_str(), entry.size());

    recordGenericCreate(key, entry);
}

void Recorder::recordSet(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            objectType,
            1,
            attr,
            false);

    auto serializedObjectType  = sai_serialize_object_type(objectType);

    auto key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic set key: %s, fields: %lu", key.c_str(), entry.size());

    recordGenericSet(key, entry);
}

void Recorder::recordGet(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            objectType,
            attr_count,
            attr_list,
            false);

    std::string serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic get key: %s, fields: %lu", key.c_str(), entry.size());

    recordGenericGet(key, entry);
}

void Recorder::recordGenericGetResponse(
        _In_ sai_status_t status,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (status == SAI_STATUS_SUCCESS)
    {
        auto entry = SaiAttributeList::serialize_attr_list(
            objectType,
            attr_count,
            attr_list,
            false);

        recordGenericGetResponse(status, entry);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        // will only record COUNT values for lists, since count is expected
        // values, and user buffer is not enough to return all from SAI

        auto entry = SaiAttributeList::serialize_attr_list(
            objectType,
            attr_count,
            attr_list,
            true);

        recordGenericGetResponse(status, entry);
    }
    else
    {
        recordGenericGetResponse(status, {});
    }
}

#define DECLARE_RECORD_REMOVE_ENTRY(OT,ot)                              \
void Recorder::recordRemove(                                            \
        _In_ const sai_ ## ot ## _t* ot)                                \
{                                                                       \
    SWSS_LOG_ENTER();                                                   \
    recordRemove((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,             \
        sai_serialize_ ## ot(*ot));                                     \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_RECORD_REMOVE_ENTRY);

#define DECLARE_RECORD_CREATE_ENTRY(OT,ot)                                                      \
void Recorder::recordCreate(                                                                    \
        _In_ const sai_ ## ot ## _t* ot,                                                        \
        _In_ uint32_t attr_count,                                                               \
        _In_ const sai_attribute_t *attr_list)                                                  \
{                                                                                               \
    SWSS_LOG_ENTER();                                                                           \
    recordCreate((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,                                     \
        sai_serialize_ ## ot(*ot), attr_count, attr_list);                                      \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_RECORD_CREATE_ENTRY);

#define DECLARE_RECORD_SET_ENTRY(OT,ot)                                                         \
void Recorder::recordSet(                                                                       \
        _In_ const sai_ ## ot ## _t* ot,                                                        \
        _In_ const sai_attribute_t *attr)                                                       \
{                                                                                               \
    SWSS_LOG_ENTER();                                                                           \
    recordSet((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,                                        \
        sai_serialize_ ## ot(*ot), attr);                                                       \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_RECORD_SET_ENTRY);

#define DECLARE_RECORD_GET_ENTRY(OT,ot)                                                         \
void Recorder::recordGet(                                                                       \
        _In_ const sai_ ## ot ## _t* ot,                                                        \
        _In_ uint32_t attr_count,                                                               \
        _In_ const sai_attribute_t *attr_list)                                                  \
{                                                                                               \
    SWSS_LOG_ENTER();                                                                           \
    recordGet((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,                                        \
        sai_serialize_ ## ot(*ot), attr_count, attr_list);                                      \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_RECORD_GET_ENTRY);

void Recorder::recordObjectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    auto key = sai_serialize_object_type(SAI_OBJECT_TYPE_SWITCH) + ":" + sai_serialize_object_id(switchId);

    auto values = SaiAttributeList::serialize_attr_list(
        objectType,
        attrCount,
        attrList,
        false);

    SWSS_LOG_DEBUG("Query arguments: switch: %s, attributes: %s", key.c_str(), Globals::joinFieldValues(values).c_str());

    recordObjectTypeGetAvailability(key, values);
}

void Recorder::recordObjectTypeGetAvailabilityResponse(
        _In_ sai_status_t status,
        _In_ const uint64_t *count)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    values.push_back(swss::FieldValueTuple("COUNT", std::to_string(*count)));

    recordObjectTypeGetAvailabilityResponse(status, values);
}

void Recorder::recordQueryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *Capability)
{
    SWSS_LOG_ENTER();

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d",
                sai_serialize_object_type(objectType).c_str(), attrId);
        return;
    }

    auto key = sai_serialize_object_type(SAI_OBJECT_TYPE_SWITCH) + ":" + sai_serialize_object_id(switchId);

    auto object_type_str = sai_serialize_object_type(objectType);
    const std::string attr_id_str = meta->attridname;
    const std::vector<swss::FieldValueTuple> values =
    {
        swss::FieldValueTuple("OBJECT_TYPE", object_type_str),
        swss::FieldValueTuple("ATTR_ID", attr_id_str)
    };

    SWSS_LOG_DEBUG("Query arguments: switch %s, object_type: %s, attribute: %s",
                key.c_str(),
                object_type_str.c_str(),
                meta->attridname);

    recordQueryAttributeCapability(key, values);
}

void Recorder::recordQueryAttributeCapabilityResponse(
        _In_ sai_status_t status,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _In_ const sai_attr_capability_t* capability)
{
    SWSS_LOG_ENTER();

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d",
                sai_serialize_object_type(objectType).c_str(), attrId);
        return;
    }

    auto object_type_str = sai_serialize_object_type(objectType);
    const std::string attr_id_str = meta->attridname;
    const std::string create_str = (status == SAI_STATUS_SUCCESS ? (capability->create_implemented? "true":"false"): "false");
    const std::string set_str    = (status == SAI_STATUS_SUCCESS ? (capability->set_implemented? "true":"false"): "false");
    const std::string get_str    = (status == SAI_STATUS_SUCCESS ? (capability->get_implemented? "true":"false"): "false");
    const std::vector<swss::FieldValueTuple> values =
    {
        swss::FieldValueTuple("OBJECT_TYPE", object_type_str),
        swss::FieldValueTuple("ATTR_ID", attr_id_str),
        swss::FieldValueTuple("CREATE_IMP", create_str),
        swss::FieldValueTuple("SET_IMP", set_str),
        swss::FieldValueTuple("GET_IMP", get_str)
    };

    recordQueryAttributeCapabilityResponse(status, values);
}

void Recorder::recordQueryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enumValuesCapability)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d",
                sai_serialize_object_type(objectType).c_str(), attrId);
        return;
    }

    if (!meta->enummetadata)
    {
        SWSS_LOG_ERROR("Attribute %s is not enum/enumlist!", meta->attridname);
        return;
    }

    auto key = sai_serialize_object_type(SAI_OBJECT_TYPE_SWITCH) + ":" + sai_serialize_object_id(switchId);

    auto str_attr_id = sai_serialize_attr_id(*meta);
    auto str_enum_list = sai_serialize_enum_list(*enumValuesCapability, meta->enummetadata, true); // we only need to serialize count

    values.emplace_back(str_attr_id, str_enum_list);

    SWSS_LOG_DEBUG("Query arguments: switch %s, attribute: %s, count: %u", key.c_str(), meta->attridname, enumValuesCapability->count);

    recordQueryAttributeEnumValuesCapability(key, values);
}

void Recorder::recordQueryAttributeEnumValuesCapabilityResponse(
        _In_ sai_status_t status,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _In_ const sai_s32_list_t* enumValuesCapability)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d",
                sai_serialize_object_type(objectType).c_str(),
                attrId);

        return;
    }

    if (!meta->enummetadata)
    {
        SWSS_LOG_ERROR("Attribute %s is not enum/enumlist!", meta->attridname);
        return;
    }

    bool countOnly = (status == SAI_STATUS_BUFFER_OVERFLOW);

    if (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        auto str_attr_id = sai_serialize_attr_id(*meta);
        auto str_enum_list = sai_serialize_enum_list(*enumValuesCapability, meta->enummetadata, countOnly);

        values.emplace_back(str_attr_id, str_enum_list);
    }

    recordQueryAttributeEnumValuesCapabilityResponse(status, values);
}

void Recorder::recordQueryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _Inout_ sai_stat_capability_list_t* stats_capability)
{
    SWSS_LOG_ENTER();

    auto key = sai_serialize_object_type(SAI_OBJECT_TYPE_SWITCH) + ":" + sai_serialize_object_id(switchId);

    auto object_type_str = sai_serialize_object_type(object_type);
    const std::string list_size = std::to_string(stats_capability->count);
    const std::vector<swss::FieldValueTuple> values =
    {
        swss::FieldValueTuple("OBJECT_TYPE", object_type_str),
        swss::FieldValueTuple("LIST_SIZE", list_size)
    };

    SWSS_LOG_DEBUG("Query arguments: switch %s, object_type: %s, count: %s",
                key.c_str(),
                object_type_str.c_str(),
                list_size.c_str());

    recordQueryStatsCapability(key, values);
}

void Recorder::recordQueryStatsCapabilityResponse(
        _In_ sai_status_t status,
        _In_ sai_object_type_t objectType,
        _In_ const sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    std::string str_stats_list;

    auto meta = sai_metadata_get_object_type_info(objectType);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find object metadata: object type %s",
                sai_serialize_object_type(objectType).c_str());

        return;
    }

    if (meta->statenum == NULL)
    {
        SWSS_LOG_ERROR("%s does not support stats", meta->objecttypename);

        return;
    }

    bool countOnly = (status == SAI_STATUS_BUFFER_OVERFLOW);

    if (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        str_stats_list = sai_serialize_stats_capability_list(*stats_capability, meta->statenum, countOnly);
    }

    recordQueryStatsCapabilityResponse(status, str_stats_list);
}

void Recorder::recordQueryStatsStCapability(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    auto key = sai_serialize_object_type(SAI_OBJECT_TYPE_SWITCH) + ":" + sai_serialize_object_id(switch_id);

    auto object_type_str = sai_serialize_object_type(object_type);
    const std::string list_size = std::to_string(stats_capability->count);
    const std::vector<swss::FieldValueTuple> values =
        {
            swss::FieldValueTuple("OBJECT_TYPE", object_type_str),
            swss::FieldValueTuple("LIST_SIZE", list_size)};

    SWSS_LOG_DEBUG("Query arguments: switch %s, object_type: %s, count: %s",
                   key.c_str(),
                   object_type_str.c_str(),
                   list_size.c_str());

    recordQueryStatsStCapability(key, values);
}

void Recorder::recordQueryStatsStCapabilityResponse(
    _In_ sai_status_t status,
    _In_ sai_object_type_t objectType,
    _In_ const sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    std::string str_stats_list;

    auto meta = sai_metadata_get_object_type_info(objectType);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find object metadata: object type %s",
                       sai_serialize_object_type(objectType).c_str());

        return;
    }

    if (meta->statenum == NULL)
    {
        SWSS_LOG_ERROR("%s does not support stats", meta->objecttypename);

        return;
    }

    bool countOnly = (status == SAI_STATUS_BUFFER_OVERFLOW);

    if (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        str_stats_list = sai_serialize_stats_st_capability_list(*stats_capability, meta->statenum, countOnly);
    }

    recordQueryStatsStCapabilityResponse(status, str_stats_list);
}

void Recorder::recordNotifySyncd(
        _In_ sai_object_id_t switchId,
        _In_ sai_redis_notify_syncd_t redisNotifySyncd)
{
    SWSS_LOG_ENTER();

    recordNotifySyncd(sai_serialize(redisNotifySyncd));
}

void Recorder::recordStats(
        _In_ bool enable)
{
    SWSS_LOG_ENTER();

    m_recordStats = enable;
}


void Recorder::recordGenericResponse(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    if (status != SAI_STATUS_SUCCESS)
    {
        // record only when response is not success

        recordLine("E|" + sai_serialize_status(status));
    }
}

void Recorder::recordBulkGenericResponse(
        _In_ sai_status_t status,
        _In_ uint32_t objectCount,
        _In_ const sai_status_t *objectStatuses)
{
    SWSS_LOG_ENTER();

    if (status != SAI_STATUS_SUCCESS)
    {
        // record only when response is not success

        std::string statuses = "";

        for (uint32_t i = 0; i < objectCount; i++)
        {
            statuses += "|" + sai_serialize_status(objectStatuses[i]);
        }

        recordLine("E|" + sai_serialize_status(status) + "|" + statuses);
    }
}

void Recorder::recordComment(
        _In_ const std::string& comment)
{
    SWSS_LOG_ENTER();

    recordLine("#|" + comment);
}
