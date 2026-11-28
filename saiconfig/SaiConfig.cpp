#include "SaiConfig.h"
#include "swss/logger.h"
#include "swss/dbconnector.h"
#include "swss/rediscommand.h"
#include "swss/redispipeline.h"

#include "ContextConfigContainer.h"
#include "ZeroMQChannel.h"

#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cctype>

using namespace std;
using namespace swss;
using namespace syncd;
using namespace sairedis;
using namespace std::chrono;
using namespace std::this_thread;
using json = nlohmann::json;

const string ASIC_STATE_KEY_VALUE_OP_QUEUE = "ASIC_STATE_KEY_VALUE_OP_QUEUE";
const string ASIC_STATE_CHANNEL = "ASIC_STATE_CHANNEL@1";
const string CONTEXT_CFG_FILE = "/usr/share/sonic/hwsku/context_config.json";

SaiConfigManager::SaiConfigManager()
    : m_oidManager(nullptr), m_switchId(0)
{
    SWSS_LOG_ENTER();

    // Initialize composite key entry definitions
    m_compositeKeyEntries = {
        {"route_entry", {"dest", "vr"}},
        {"fdb_entry", {"mac", "bvid"}},
        {"neighbor_entry", {"rif", "ip"}}
    };
}

SaiConfigManager::~SaiConfigManager()
{
    SWSS_LOG_ENTER();
}

bool SaiConfigManager::initialize()
{
    SWSS_LOG_ENTER();

    try
    {
        auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);

        // Create switch configuration
        auto switchConfig = std::make_shared<SwitchConfig>(0, "");

        auto switchConfigContainer = std::make_shared<SwitchConfigContainer>();
        switchConfigContainer->insert(switchConfig);

        auto redisVidIndexGenerator = std::make_shared<sairedis::RedisVidIndexGenerator>(dbAsic, REDIS_KEY_VIDCOUNTER);

        // Create VirtualObjectIdManager
        m_oidManager = std::make_shared<VirtualObjectIdManager>(0, switchConfigContainer, redisVidIndexGenerator);

        // Generate switch ID
        m_switchId = m_oidManager->allocateNewSwitchObjectId("");

        SWSS_LOG_NOTICE("SaiConfigManager initialized with switch ID: 0x%" PRIx64, m_switchId);
        return true;
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Failed to initialize SaiConfigManager: %s", e.what());
        return false;
    }
}

string SaiConfigManager::generateSaiObjectId(const string &objectName)
{
    SWSS_LOG_ENTER();

    try
    {
        sai_object_type_t objectType = getSaiObjectTypeEnum(objectName);
        if (objectType == SAI_OBJECT_TYPE_NULL)
        {
            SWSS_LOG_ERROR("Unknown object type for %s", objectName.c_str());
            return "";
        }
        else if (objectType == SAI_OBJECT_TYPE_SWITCH)
        {
            stringstream ss;
            ss << "oid:0x" << hex << m_switchId;
            return ss.str();
        }

        sai_object_id_t oid = m_oidManager->allocateNewObjectId(objectType, m_switchId);
        stringstream ss;
        ss << "oid:0x" << hex << oid;
        return ss.str();
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Failed to generate SAI object ID for %s: %s", objectName.c_str(), e.what());
        return "";
    }
}

sai_object_id_t SaiConfigManager::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

bool SaiConfigManager::isCompositeKeyEntry(const string &objectName) const
{
    SWSS_LOG_ENTER();
    return m_compositeKeyEntries.find(objectName) != m_compositeKeyEntries.end();
}

const vector<string>* SaiConfigManager::getCompositeKeyFields(const string &objectName) const
{
    SWSS_LOG_ENTER();
    auto it = m_compositeKeyEntries.find(objectName);
    if (it != m_compositeKeyEntries.end())
    {
        return &it->second;
    }
    return nullptr;
}

sai_object_type_t SaiConfigManager::getSaiObjectTypeEnum(const string &objectName)
{
    SWSS_LOG_ENTER();

    // Convert object name to SAI object type string
    string saiObjectTypeStr = "SAI_OBJECT_TYPE_" + objectName;
    transform(saiObjectTypeStr.begin(), saiObjectTypeStr.end(), saiObjectTypeStr.begin(), ::toupper);

    // Use SAI deserialize API to convert string to enum
    sai_object_type_t objectType;
    if (sai_deserialize_object_type(saiObjectTypeStr.c_str(), &objectType) > 0)
    {
        SWSS_LOG_NOTICE("Converted %s to object type %d", saiObjectTypeStr.c_str(), objectType);
        return objectType;
    }

    SWSS_LOG_WARN("Failed to convert %s to SAI object type, using fallback", saiObjectTypeStr.c_str());
    return SAI_OBJECT_TYPE_NULL; // fallback
}

string SaiConfigManager::getSaiObjectTypeFromId(const string &objectIdStr)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid;
    string objectTypeStr;

    sai_deserialize_object_id(objectIdStr, oid);

    SWSS_LOG_NOTICE("Deserialized OID: 0x%" PRIx64, oid);

    // Get the object type enum from objectid
    sai_object_type_t objectType = m_oidManager->saiObjectTypeQuery(oid);

    // Use SAI serialize API to convert enum to string
    objectTypeStr = sai_serialize_object_type(objectType);

    return objectTypeStr;
}

// SaiConfig implementation
SaiConfig::SaiConfig()
    : m_noWait(false), m_verbose(false), m_contextConfigFile(CONTEXT_CFG_FILE)
{
    SWSS_LOG_ENTER();
    m_db = std::make_shared<swss::DBConnector>("ASIC_DB", 0, false);

    m_saiConfigManager = std::make_unique<SaiConfigManager>();

    // Initialize the manager
    if (!m_saiConfigManager->initialize())
    {
        SWSS_LOG_ERROR("Failed to initialize SaiConfigManager");
        throw std::runtime_error("Failed to initialize SaiConfigManager");
    }
}

bool SaiConfig::transportIsZmq(const string &forcedMode,
                               sairedis::context_config_zmq_state_t contextState,
                               bool configDbWantsZmq)
{
    SWSS_LOG_ENTER();

    if (forcedMode == "zmq_sync")
    {
        return true;
    }

    if (forcedMode == "redis_sync")
    {
        return false;
    }

    if (contextState == sairedis::CONTEXT_CONFIG_ZMQ_ENABLED)
    {
        return true;
    }

    if (contextState == sairedis::CONTEXT_CONFIG_ZMQ_DISABLED)
    {
        return false;
    }

    return configDbWantsZmq;
}

bool SaiConfig::configDbWantsZmq()
{
    SWSS_LOG_ENTER();

    swss::DBConnector configDb("CONFIG_DB", 0);
    string value;

    swss::Table deviceMetadata(&configDb, "DEVICE_METADATA");

    // The start script puts -z zmq_sync on a DPU unconditionally.
    if (deviceMetadata.hget("localhost", "switch_type", value) && value == "dpu")
    {
        return true;
    }

    swss::Table systemDefaults(&configDb, "SYSTEM_DEFAULTS");

    return systemDefaults.hget("swss_zmq", "status", value) && value == "enabled";
}

bool SaiConfig::initTransport()
{
    SWSS_LOG_ENTER();

    /* No channel unless we get all the way through. */
    m_channel = nullptr;

    auto ccc = sairedis::ContextConfigContainer::loadFromFile(m_contextConfigFile.c_str());

    auto cc = (ccc == nullptr) ? nullptr : ccc->get(0);

    if (cc == nullptr)
    {
        cc = sairedis::ContextConfigContainer::getDefault()->get(0);
    }

    bool fromConfigDb = false;

    if (m_forcedMode.empty() && cc->m_zmqEnable == sairedis::CONTEXT_CONFIG_ZMQ_EMPTY)
    {
        fromConfigDb = configDbWantsZmq();
    }

    if (!transportIsZmq(m_forcedMode, cc->m_zmqEnable, fromConfigDb))
    {
        SWSS_LOG_NOTICE("using the redis queue path (mode '%s', context %s, config_db zmq %s)",
                        m_forcedMode.c_str(), m_contextConfigFile.c_str(), fromConfigDb ? "on" : "off");
        return false;
    }

    m_channel = std::make_shared<sairedis::ZeroMQChannel>(cc->m_zmqEndpoint);

    SWSS_LOG_NOTICE("using zmq channel to syncd, endpoint %s (mode '%s', context %s, config_db zmq %s)",
                    cc->m_zmqEndpoint.c_str(), m_forcedMode.c_str(), m_contextConfigFile.c_str(),
                    fromConfigDb ? "on" : "off");

    return true;
}

SaiConfig::~SaiConfig()
{
    SWSS_LOG_ENTER();
}

void SaiConfig::printUsage()
{
    SWSS_LOG_ENTER();

    cout << "Usage: saiconfig [OPTIONS] [FILE...]" << endl;
    cout << "Options:" << endl;
    cout << "  -h, --help       Show this help message" << endl;
    cout << "  -n --no-wait     Don't wait for SAI response (fire and forget mode)" << endl;
    cout << "  -v, --verbose    Enable verbose logging (NOTICE level)" << endl;
    cout << "  -z, --mode <mode>             Transport to syncd: redis_sync or zmq_sync (default: same choice syncd made)" << endl;
    cout << "  -x, --context-config <file>   Context config file (default: " << CONTEXT_CFG_FILE << ")" << endl;
}

void SaiConfig::handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();
    vector<string> files;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printUsage();
            return;
        }
        else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--no-wait") == 0)
        {
            m_noWait = true;
            SWSS_LOG_NOTICE("Don't wait mode enabled - will not wait for SAI responses");
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            m_verbose = true;
            // Enable verbose logging (NOTICE level)
            swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
            SWSS_LOG_NOTICE("Verbose logging enabled");
        }
        else if (strcmp(argv[i], "-z") == 0 || strcmp(argv[i], "--mode") == 0)
        {
            if (i + 1 >= argc)
            {
                throw invalid_argument("-z needs a mode: redis_sync or zmq_sync");
            }

            m_forcedMode = argv[++i];

            if (m_forcedMode != "redis_sync" && m_forcedMode != "zmq_sync")
            {
                throw invalid_argument("unknown mode '" + m_forcedMode + "'; expected redis_sync or zmq_sync");
            }
        }
        else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--context-config") == 0)
        {
            if (i + 1 >= argc)
            {
                throw invalid_argument("-x needs a file path");
            }

            m_contextConfigFile = argv[++i];
        }
        else
        {
            // Treat as a file
            files.push_back(argv[i]);
        }
    }

    initTransport();

    for (const auto &file : files)
    {
        SWSS_LOG_NOTICE("Processing config file: %s", file.c_str());

        if (!processJsonFile(file))
        {
            SWSS_LOG_ERROR("Failed to process file: %s", file.c_str());
            cerr << "Failed to process file: " << file << endl;
            throw std::runtime_error("Failed to process config file");
        }
    }
}

string SaiConfig::getSaiObjectType(const string &objectName)
{
    SWSS_LOG_ENTER();

    // Convert object name to SAI object type string generically
    string upperName = objectName;
    transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    return "SAI_OBJECT_TYPE_" + upperName;
}

bool SaiConfig::isOidFormat(const string &str)
{
    SWSS_LOG_ENTER();

    // Check if string starts with "oid:0x"
    if (str.find("oid:0x") == 0 && str.length() > 6)
    {
        return true;
    }
    return false;
}

string SaiConfig::resolveOidValue(const string &oidValue)
{
    SWSS_LOG_ENTER();

    if (oidValue.empty())
    {
        SWSS_LOG_ERROR("OID_VALUE field is empty");
        return "";
    }

    // Literal OID string (e.g., "oid:0x...")
    if (isOidFormat(oidValue))
    {
        return oidValue;
    }

    // Variable reference (e.g., "$port")
    if (isVariableReference(oidValue))
    {
        return resolveVariables(oidValue);
    }

    // Any other form is invalid for the OID_VALUE field
    SWSS_LOG_ERROR("Invalid OID_VALUE field value '%s'; expected 'oid:0x...' or '$name'", oidValue.c_str());
    return "";
}

string SaiConfig::checkSaiResponse(const string &operation, int maxAttempts, int timeoutMs)
{
    SWSS_LOG_ENTER();

    try
    {
        vector<string> status;
        int attempts = maxAttempts;

        // Get response from queue
        while (attempts > 0)
        {
            // Check queue length first
            RedisCommand lenCmd;
            lenCmd.format("LLEN %s", "GETRESPONSE_KEY_VALUE_OP_QUEUE");
            RedisReply lenReply(m_db.get(), lenCmd, REDIS_REPLY_INTEGER);

            long long queueLen = 0;
            if (lenReply.getContext() && lenReply.getContext()->type == REDIS_REPLY_INTEGER)
            {
                queueLen = lenReply.getContext()->integer;
            }

            if (queueLen >= 3)
            {
                // Get all elements from the response queue
                RedisCommand cmd;
                cmd.format("LRANGE %s %d %d", "GETRESPONSE_KEY_VALUE_OP_QUEUE", 0, -1);

                RedisReply reply(m_db.get(), cmd, REDIS_REPLY_ARRAY);
                if (reply.getContext() && reply.getContext()->type == REDIS_REPLY_ARRAY && reply.getContext()->elements >= 3)
                {
                    status.clear();
                    for (size_t i = 0; i < reply.getContext()->elements; i++)
                    {
                        if (reply.getContext()->element[i] && reply.getContext()->element[i]->str)
                        {
                            status.push_back(string(reply.getContext()->element[i]->str));
                        }
                    }

                    SWSS_LOG_INFO("Got SAI response with %zu elements", status.size());
                    break;
                }
            }

            // Wait before retrying
            sleep_for(milliseconds(timeoutMs));
            attempts--;
        }

        // Clear the response queue
        RedisCommand delCmd;
        delCmd.format("DEL %s", "GETRESPONSE_KEY_VALUE_OP_QUEUE");
        RedisReply delReply(m_db.get(), delCmd, REDIS_REPLY_INTEGER);

        if (status.size() > 0)
        {
            SWSS_LOG_INFO("SAI response: op=%s, key=%s, status=%s",
                          status[0].c_str(),
                          status.size() > 1 ? status[1].c_str() : "N/A",
                          status.size() > 2 ? status[2].c_str() : "N/A");
        }

        if (status.size() != 3)
        {
            SWSS_LOG_ERROR("SAI %s operation failure! Expected 3 response elements, got %zu",
                           operation.c_str(), status.size());
            return "";
        }

        return status[2]; // Return the status
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Failed to check SAI response: %s", e.what());
        return "";
    }
}

bool SaiConfig::writeToAsicDb(const string &objectType, const string &objectId,
                              const vector<pair<string, string>> &attributes,
                              const string &operation)
{
    SWSS_LOG_ENTER();

    try
    {
        // Create the key in the format: SAI_OBJECT_TYPE:objectId
        string key = objectType + ":" + objectId;

        return (m_channel != nullptr)
            ? writeToAsicDbViaZmq(key, attributes, operation)
            : writeToAsicDbViaRedis(objectType, objectId, key, attributes, operation);
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Failed to write to ASIC_DB: %s", e.what());
        cout << "Exception writing to ASIC_DB: " << e.what() << endl;
        return false;
    }
}

bool SaiConfig::writeToAsicDbViaZmq(const string &key,
                                    const vector<pair<string, string>> &attributes,
                                    const string &operation)
{
    SWSS_LOG_ENTER();

    vector<swss::FieldValueTuple> values;

    values.reserve(attributes.size());

    for (const auto &attr : attributes)
    {
        values.emplace_back(attr.first, attr.second);
    }

    m_channel->set(key, values, operation);

    // REQ/REP is strict send-then-recv, so the reply must always be
    // read; --no-wait is not honored on this path.
    swss::KeyOpFieldsValuesTuple kco;

    sai_status_t status = m_channel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("SAI %s operation failed for %s with status: %s",
                       operation.c_str(), key.c_str(),
                       sai_serialize_status(status).c_str());
        cout << "SAI " << operation << " operation failed for " << key
             << " with status: " << sai_serialize_status(status) << endl;
        return false;
    }

    SWSS_LOG_NOTICE("Successfully %s SAI object over zmq: %s",
                    operation.c_str(), key.c_str());

    return true;
}

bool SaiConfig::writeToAsicDbViaRedis(const string &objectType, const string &objectId,
                                      const string &key,
                                      const vector<pair<string, string>> &attributes,
                                      const string &operation)
{
    SWSS_LOG_ENTER();

    RedisPipeline pipeline(m_db.get());

    // Create the value as a JSON array with alternating attribute names and values
    json valueArray = json::array();
    for (const auto &attr : attributes)
    {
        valueArray.push_back(attr.first);  // attribute name
        valueArray.push_back(attr.second); // attribute value
    }
    string value = valueArray.dump();

    // Create Redis command to push to ASIC_STATE_KEY_VALUE_OP_QUEUE
    // Format: LPUSH ASIC_STATE_KEY_VALUE_OP_QUEUE key value "S{operation}"
    string opCode = "S" + operation;

    RedisCommand queueCommand;
    queueCommand.format("LPUSH %s %s %s %s",
                        ASIC_STATE_KEY_VALUE_OP_QUEUE.c_str(),
                        key.c_str(),
                        value.c_str(),
                        opCode.c_str());

    pipeline.push(queueCommand, REDIS_REPLY_INTEGER);

    // Publish notification
    RedisCommand publishCommand;
    publishCommand.format("PUBLISH %s %s",
                          ASIC_STATE_CHANNEL.c_str(),
                          "G");

    pipeline.push(publishCommand, REDIS_REPLY_INTEGER);

    // Execute the pipeline
    pipeline.flush();

    SWSS_LOG_NOTICE("SAI operation submitted for object %s:%s", objectType.c_str(), objectId.c_str());

    // Check SAI response (unless --no-wait is specified)
    if (m_noWait)
    {
        SWSS_LOG_NOTICE("Don't wait mode - skipping response check for %s operation on %s:%s",
                       operation.c_str(), objectType.c_str(), objectId.c_str());
        return true;
    }

    string response = checkSaiResponse(operation);
    if (response.empty())
    {
        SWSS_LOG_ERROR("Failed to get SAI response for %s operation on %s:%s",
                       operation.c_str(), objectType.c_str(), objectId.c_str());
        cout << "Failed to get SAI response for " << operation << " operation on " << key << endl;
        return false;
    }

    if (response != "SAI_STATUS_SUCCESS")
    {
        SWSS_LOG_ERROR("SAI %s operation failed for %s:%s with status: %s",
                       operation.c_str(), objectType.c_str(), objectId.c_str(), response.c_str());
        cout << "SAI " << operation << " operation failed for " << key << " with status: " << response << endl;
        return false;
    }

    SWSS_LOG_NOTICE("Successfully wrote SAI object %s:%s to ASIC_DB", objectType.c_str(), objectId.c_str());
    SWSS_LOG_NOTICE("Successfully %s SAI object: %s, Status: %s, Value: %s, OP: %s", operation.c_str(), key.c_str(), response.c_str(),
     value.c_str(), opCode.c_str());
    return true;
}

bool SaiConfig::readFromAsicDb(const string &objectType, const string &objectId,
                               const vector<pair<string, string>> &attributes)
{
    SWSS_LOG_ENTER();

    try
    {
        RedisCommand cmd;
        string key = "ASIC_STATE:" + objectType + ":" + objectId;

        if (attributes.empty()) {
            cmd.format("HGETALL %s", key.c_str());
        } else {
            // Build HMGET command with key and each field as separate arguments
            vector<string> args = {"HMGET", key};
            for (const auto& attr : attributes) {
                args.push_back(attr.first);
            }
            cmd.format(args);
        }

        RedisReply reply(m_db.get(), cmd, REDIS_REPLY_ARRAY);

        if (reply.getContext() && reply.getContext()->type == REDIS_REPLY_ARRAY)
        {
            // Collect all attribute-value pairs first
            vector<pair<string, string>> results;

            if (attributes.empty()) {
                // HGETALL returns alternating field names and values
                for (size_t i = 0; i < reply.getContext()->elements; i += 2)
                {
                    if (i + 1 < reply.getContext()->elements)
                    {
                        results.emplace_back(
                            reply.getContext()->element[i]->str,
                            reply.getContext()->element[i + 1]->str);
                    }
                }
            } else {
                // HMGET returns values, in the same order as requested
                for (size_t i = 0; i < reply.getContext()->elements && i < attributes.size(); i++)
                {
                    const char* value = reply.getContext()->element[i]->str;
                    results.emplace_back(
                        attributes[i].first,
                        value != nullptr ? value : "(nil)");
                }
            }

            // Calculate column width for alignment
            size_t attrWidth = 9;  // "Attribute"
            for (const auto& r : results)
            {
                attrWidth = max(attrWidth, r.first.length());
            }

            // Print aligned columns
            cout << endl;
            cout << "GET " << objectType << ":" << objectId << " (" << results.size() << " attributes)" << endl;
            cout << "Attribute" << string(attrWidth - 9 + 2, ' ') << "Value" << endl;
            for (const auto& r : results)
            {
                cout << r.first << string(attrWidth - r.first.length() + 2, ' ') << r.second << endl;
            }

            return true;
        }

        return false;
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Failed to read from ASIC_DB: %s", e.what());
        cout << "Exception reading from ASIC_DB: " << e.what() << endl;
        return false;
    }
}

bool SaiConfig::isVariableReference(const string &value)
{
    SWSS_LOG_ENTER();

    return (!value.empty() && value[0] == '$');
}

string SaiConfig::resolveVariables(const string &value)
{
    SWSS_LOG_ENTER();

    // Fast path: no variables to resolve
    if (value.find('$') == string::npos)
    {
        return value;
    }

    string result = value;
    size_t pos = 0;

    while ((pos = result.find('$', pos)) != string::npos)
    {
        size_t start = pos + 1;
        size_t end = start;

        // Variable name characters: alphanumeric, underscore, and dash
        while (end < result.size() &&
               (std::isalnum(static_cast<unsigned char>(result[end])) ||
                result[end] == '_' ||
                result[end] == '-'))
        {
            ++end;
        }

        if (end == start)
        {
            SWSS_LOG_THROW("Invalid variable reference syntax near pos %zu in '%s'",
                           pos, result.c_str());
        }

        string varName = result.substr(start, end - start);
        auto it = m_objectIdMap.find(varName);
        if (it == m_objectIdMap.end())
        {
            SWSS_LOG_THROW("Undefined variable: %s", varName.c_str());
        }

        SWSS_LOG_NOTICE("Resolved variable $%s to %s", varName.c_str(), it->second.c_str());

        // Replace "$varName" with the resolved OID
        result.replace(pos, 1 + varName.size(), it->second);

        // Advance position past the replacement
        pos += it->second.size();
    }

    return result;
}

string SaiConfig::buildCompositeKeyJson(const string &objectName, const json &objectData, const string &operation)
{
    SWSS_LOG_ENTER();

    const auto* keyFields = m_saiConfigManager->getCompositeKeyFields(objectName);
    if (!keyFields)
    {
        SWSS_LOG_ERROR("No composite key fields found for %s", objectName.c_str());
        return "";
    }

    json entryKey;

    // Auto-inject switch_id
    stringstream ss;
    ss << "oid:0x" << hex << m_saiConfigManager->getSwitchId();
    entryKey["switch_id"] = ss.str();

    // Process each key field
    for (const auto& fieldName : *keyFields)
    {
        if (!objectData.contains(fieldName))
        {
            SWSS_LOG_ERROR("%s missing required '%s' field for %s",
                objectName.c_str(), fieldName.c_str(), operation.c_str());
            return "";
        }

        string value = objectData[fieldName].get<string>();
        entryKey[fieldName] = resolveVariables(value);
    }

    return entryKey.dump();
}

void SaiConfig::requireObjectInAsicDb(const string &objectType, const string &objectId,
                                      const string &what)
{
    SWSS_LOG_ENTER();

    string key = "ASIC_STATE:" + objectType + ":" + objectId;

    if (!m_db->exists(key))
    {
        throw invalid_argument(what + " " + key + " does not exist in ASIC_DB");
    }
}

void SaiConfig::requireObjectNotInAsicDb(const string &objectType, const string &objectId)
{
    SWSS_LOG_ENTER();

    string key = "ASIC_STATE:" + objectType + ":" + objectId;

    if (m_db->exists(key))
    {
        throw invalid_argument("create target " + key + " already exists in ASIC_DB");
    }
}

void SaiConfig::validateRequest(const string &objectType,
                                const string &objectId,
                                const vector<pair<string, string>> &attributes,
                                const string &operation)
{
    SWSS_LOG_ENTER();

    sai_object_type_t ot = SAI_OBJECT_TYPE_NULL;

    sai_deserialize_object_type(objectType, ot);

    for (const auto &attr : attributes)
    {
        const string &name = attr.first;
        const string &value = attr.second;

        auto md = sai_metadata_get_attr_metadata_by_attr_id_name(name.c_str());

        if (md == nullptr)
        {
            throw invalid_argument("unknown attribute '" + name + "'");
        }

        if (md->objecttype != ot)
        {
            throw invalid_argument("attribute '" + name + "' does not belong to " + objectType);
        }

        if (operation == "get")
        {
            continue; // attribute names only; values are not sent
        }

        // An OID pointing at an object syncd never created fails VID-to-RID
        // translation inside syncd, same as a bad target. Lists are
        // serialized as "<count>:oid1,oid2,...".
        if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            requireReferencedOid(value, "attribute " + name);
        }
        else if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            size_t colon = value.find(':');
            string items = (colon == string::npos) ? value : value.substr(colon + 1);
            stringstream ss(items);
            string item;
            while (getline(ss, item, ','))
            {
                requireReferencedOid(item, "attribute " + name);
            }
        }
    }

    // Entry keys (route vr, neighbor rif, fdb bvid) carry OIDs that syncd
    // translates before the SAI call, with the same failure mode.
    if (!objectId.empty() && objectId[0] == '{')
    {
        json key = json::parse(objectId);
        for (auto it = key.begin(); it != key.end(); ++it)
        {
            if (it.key() == "switch_id" || !it.value().is_string())
            {
                continue;
            }
            requireReferencedOid(it.value().get<string>(), "key field " + it.key());
        }
    }

    // create is the one operation whose target must NOT exist yet. For the
    // others it must: a set or remove on a missing VID is fatal to syncd, and
    // a get on a missing key just prints (nil) for every attribute, which
    // hides a typo in the OID or a wrong object type.
    if (operation != "create")
    {
        requireObjectInAsicDb(objectType, objectId, operation + " target");
    }
    else if (!objectId.empty() && objectId[0] == '{')
    {
        // An entry key the user wrote may already be programmed; SAI rejects
        // the duplicate. OID objects need no such check, their VID is fresh.
        requireObjectNotInAsicDb(objectType, objectId);
    }

    if (operation != "get")
    {
        requireDeserializable(objectType, objectId, attributes);
    }
}

void SaiConfig::requireDeserializable(const string &objectType, const string &objectId,
                                      const vector<pair<string, string>> &attributes)
{
    SWSS_LOG_ENTER();

    sai_object_meta_key_t metaKey;

    try
    {
        sai_deserialize_object_meta_key(objectType + ":" + objectId, metaKey);
    }
    catch (const exception &e)
    {
        throw invalid_argument("malformed object key " + objectId + ": " + e.what());
    }

    vector<swss::FieldValueTuple> values;

    for (const auto &attr : attributes)
    {
        values.emplace_back(attr.first, attr.second);
    }

    try
    {
        // Same constructor syncd runs on receipt; throws on any value it
        // cannot parse against the attribute's metadata.
        saimeta::SaiAttributeList list(metaKey.objecttype, values, false);
    }
    catch (const exception &e)
    {
        throw invalid_argument(string("malformed attribute value: ") + e.what());
    }
}

void SaiConfig::requireReferencedOid(const string &value, const string &what)
{
    SWSS_LOG_ENTER();

    if (!isOidFormat(value) || value == "oid:0x0")
    {
        return; // null OID or not an OID at all; left to SAI to judge
    }

    requireObjectInAsicDb(m_saiConfigManager->getSaiObjectTypeFromId(value), value,
                          what + " references");
}

void SaiConfig::requireSyncdReady()
{
    SWSS_LOG_ENTER();

    // Right after a (re)start syncd has not yet rebuilt its VID-to-RID map,
    // and a request for an otherwise valid OID is fatal to it. The switch
    // itself is the first entry it publishes.
    stringstream ss;
    ss << "oid:0x" << hex << m_saiConfigManager->getSwitchId();

    if (!m_db->hexists("VIDTORID", ss.str()))
    {
        throw runtime_error("syncd has not published its switch in VIDTORID yet; it is still starting, retry later");
    }
}

bool SaiConfig::processJsonFile(const string &filename)
{
    SWSS_LOG_ENTER();

    try
    {
        ifstream file(filename);
        if (!file.is_open())
        {
            SWSS_LOG_ERROR("Failed to open file: %s", filename.c_str());
            return false;
        }

        json jsonData;
        file >> jsonData;

        if (!jsonData.is_array())
        {
            SWSS_LOG_ERROR("Root element must be an array in file: %s", filename.c_str());
            return false;
        }

        if (!jsonData.empty())
        {
            requireSyncdReady();
        }

        // Process each item in the JSON array
        for (size_t i = 0; i < jsonData.size(); i++)
        {
            const auto &arrayItem = jsonData[i];

            if (!arrayItem.is_object())
            {
                SWSS_LOG_ERROR("Array item %zu must be a JSON object", i);
                return false;
            }

	        // Extract operation, optional OID_VALUE/OID_NAME fields, and object definition
	        string operation;
	        string objectName;
	        string nameField; // OID_NAME for creates
	        string oidValueField;  // OID_VALUE for sets
	        json objectData;

	        // Parse the array item to extract object name, data, operation, OID_VALUE, and OID_NAME
	        for (auto it = arrayItem.begin(); it != arrayItem.end(); ++it)
	        {
	            if (it.key() == "OP")
	            {
	                operation = it.value().get<string>();
	                // Convert to lowercase for consistency
	                transform(operation.begin(), operation.end(), operation.begin(), ::tolower);
	            }
	            else if (it.key() == "OID_NAME")
	            {
	                nameField = it.value().get<string>();
	            }
	            else if (it.key() == "OID_VALUE")
	            {
	                if (!it.value().is_string())
	                {
	                    SWSS_LOG_ERROR("OID_VALUE field for array item %zu must be a string", i);
	                    return false;
	                }
	                oidValueField = it.value().get<string>();
	            }
	            else
	            {
	                if (!objectName.empty())
	                {
	                    SWSS_LOG_ERROR("Array item %zu contains multiple object definitions; only one object per item is supported", i);
	                    return false;
	                }
	                objectName = it.key();
	                objectData = it.value();
	            }
	        }

            if (operation.empty())
            {
                SWSS_LOG_ERROR("Array item %zu missing operation", i);
                return false;
            }

            // get and remove take object names and object data optionally.
            if (operation != "get" && operation != "remove" && objectName.empty())
            {
                SWSS_LOG_ERROR("Array item %zu missing object definition", i);
                return false;
            }

            if (operation != "get" && operation != "remove" && !objectData.is_object())
            {
                SWSS_LOG_ERROR("Object %s must be a JSON object", objectName.c_str());
                return false;
            }

	        string objectId;
	        string objectType;

	        if (operation == "create")
	        {
	            if (!oidValueField.empty())
	            {
	                SWSS_LOG_ERROR("Array item %zu has OP 'create' but also an OID_VALUE field; OID_VALUE is not allowed for create operations", i);
	                return false;
	            }

                if (!nameField.empty() && m_saiConfigManager->isCompositeKeyEntry(objectName))
                {
                    throw invalid_argument("OID_NAME is not supported for " + objectName +
                                           "; entry types are addressed by their key fields");
                }

                if (!nameField.empty())
                {
                    auto prev = m_objectIdMap.find(nameField);

                    if (prev != m_objectIdMap.end())
                    {
                        throw invalid_argument("OID_NAME '" + nameField + "' is already bound to " +
                                               prev->second + "; remove that object first or pick another name");
                    }
                }

                // Check if this is a composite key entry type
                if (m_saiConfigManager->isCompositeKeyEntry(objectName))
                {
                    objectId = buildCompositeKeyJson(objectName, objectData, operation);
                    if (objectId.empty())
                    {
                        return false;
                    }
                    objectType = getSaiObjectType(objectName);
                    SWSS_LOG_NOTICE("Created %s key: %s", objectName.c_str(), objectId.c_str());
                }
                else
                {
                    // Generate SAI object ID using VirtualObjectIdManager (for regular objects)
                    objectId = m_saiConfigManager->generateSaiObjectId(objectName);
                    if (objectId.empty())
                    {
                        SWSS_LOG_ERROR("Failed to generate SAI object ID for %s", objectName.c_str());
                        return false;
                    }

                    // Get SAI object type
                    objectType = getSaiObjectType(objectName);

                    // Store object ID if OID_NAME field is provided
                    if (!nameField.empty())
                    {
                        m_objectIdMap[nameField] = objectId;
                        SWSS_LOG_NOTICE("Stored object ID %s with name '%s'", objectId.c_str(), nameField.c_str());
                    }
                }
            }
	        else if (operation == "set" || operation == "get" || operation == "remove")
	        {
	            if (!nameField.empty())
	            {
	                throw invalid_argument("OID_NAME is only meaningful on create; array item " +
	                                       to_string(i) + " has it on '" + operation + "'");
	            }

	            if (m_saiConfigManager->isCompositeKeyEntry(objectName))
	            {
	                // Warn if OID_VALUE is also provided - it will be ignored
	                if (!oidValueField.empty())
	                {
	                    SWSS_LOG_WARN("OID_VALUE '%s' provided for composite key entry %s will be ignored; using key fields instead",
	                        oidValueField.c_str(), objectName.c_str());
	                }

	                objectId = buildCompositeKeyJson(objectName, objectData, operation);
	                if (objectId.empty())
	                {
	                    return false;
	                }
	                objectType = getSaiObjectType(objectName);
	                SWSS_LOG_NOTICE("Built %s key for %s: %s", objectName.c_str(), operation.c_str(), objectId.c_str());
	            }
	            else
	            {
	                // Regular OID-based object - requires OID_VALUE
	                if (oidValueField.empty())
	                {
	                    SWSS_LOG_ERROR("Array item %zu has OP '%s' but is missing required OID_VALUE field", i, operation.c_str());
	                    return false;
	                }

	                objectId = resolveOidValue(oidValueField);
	                if (objectId.empty())
	                {
	                    SWSS_LOG_ERROR("Failed to resolve OID_VALUE '%s' for object '%s'", oidValueField.c_str(), objectName.c_str());
	                    return false;
	                }

	                SWSS_LOG_NOTICE("objectName='%s', objectId='%s'", objectName.c_str(), objectId.c_str());

	                if (objectName.empty())
	                {
	                    objectType = m_saiConfigManager->getSaiObjectTypeFromId(objectId);
	                }
	                else
	                {
	                    objectType = getSaiObjectType(objectName);
	                }
	            }
            }
	        else
	        {
	            SWSS_LOG_ERROR("Unsupported operation '%s' for array item %zu", operation.c_str(), i);
	            return false;
	        }

            // Convert attributes to vector of pairs
            vector<pair<string, string>> attributes;

            // Get composite key fields once before the loop (if applicable)
            const auto* compositeKeyFields = m_saiConfigManager->getCompositeKeyFields(objectName);

            for (auto attrIt = objectData.begin(); attrIt != objectData.end(); ++attrIt)
            {
                const string &attrName = attrIt.key();
                const auto &attrValue = attrIt.value();

                // Skip composite key fields - these are part of the object ID, not attributes
                if (attrName == "switch_id")
                {
                    continue;
                }
                if (compositeKeyFields != nullptr &&
                    find(compositeKeyFields->begin(), compositeKeyFields->end(), attrName) != compositeKeyFields->end())
                {
                    continue;
                }

                string valueStr;
                if (attrValue.is_string())
                {
                    valueStr = attrValue.get<string>();
                }
                else if (attrValue.is_number())
                {
                    valueStr = to_string(attrValue.get<int>());
                }
                else
                {
                    valueStr = attrValue.dump();
                }

                // Resolve variables in attribute values
                valueStr = resolveVariables(valueStr);

                attributes.emplace_back(attrName, valueStr);
            }

            validateRequest(objectType, objectId, attributes, operation);

            // Read from ASIC_DB if we are trying to 'get' values
            if (operation == "get") {
                if (!readFromAsicDb(objectType, objectId, attributes))
                {
                    SWSS_LOG_ERROR("Failed to read from ASIC_DB for object %s", objectName.c_str());
                    return false;
                }
            }
            // Write to ASIC_DB with the specified operation for create|set|remove
            else if (!writeToAsicDb(objectType, objectId, attributes, operation))
            {
                SWSS_LOG_ERROR("Failed to %s object %s to ASIC_DB", operation.c_str(), objectName.c_str());
                return false;
            }

            if (operation == "remove")
            {
                for (auto it = m_objectIdMap.begin(); it != m_objectIdMap.end(); )
                {
                    it = (it->second == objectId) ? m_objectIdMap.erase(it) : next(it);
                }
            }
        }

        return true;
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Exception processing file %s: %s", filename.c_str(), e.what());
        cout << "Error: " << e.what() << endl;
        return false;
    }
}
