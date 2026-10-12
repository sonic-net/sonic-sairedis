#pragma once

#include "swss/table.h"
#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <unordered_map>

extern "C"
{
#include "sai.h"
#include "saimetadata.h"
}

#include "sairediscommon.h"
#include "VirtualObjectIdManager.h"
#include "SwitchConfig.h"
#include "SwitchConfigContainer.h"
#include "ContextConfig.h"
#include "RedisVidIndexGenerator.h"
#include "Channel.h"

namespace syncd
{
    class SaiConfigManager
    {
    public:
        SaiConfigManager();
        ~SaiConfigManager();

        bool initialize();
        std::string generateSaiObjectId(const std::string &objectName);
        sai_object_id_t getSwitchId() const;
        std::string getSaiObjectTypeFromId(const std::string &objectIdStr);

        bool isCompositeKeyEntry(const std::string &objectName) const;
        const std::vector<std::string>* getCompositeKeyFields(const std::string &objectName) const;

    private:
        sai_object_type_t getSaiObjectTypeEnum(const std::string &objectName);

        std::shared_ptr<sairedis::VirtualObjectIdManager> m_oidManager;
        sai_object_id_t m_switchId;

        std::unordered_map<std::string, std::vector<std::string>> m_compositeKeyEntries;
    };

    class SaiConfig
    {
    public:
        SaiConfig();
        ~SaiConfig();

        void handleCmdLine(int argc, char **argv);
        void printUsage();

        bool processJsonFile(const std::string &filename);
        bool writeToAsicDb(const std::string &objectType, const std::string &objectId,
                           const std::vector<std::pair<std::string, std::string>> &attributes,
                           const std::string &operation);

        /* The two write paths writeToAsicDb() dispatches to. Both report their
         * own failures and return false; exceptions are handled by the caller.
         *
         * Note --no-wait applies to the redis path only: ZMQ_REQ/REP is strict
         * send-then-recv, so the zmq path must always read the reply. */
        bool writeToAsicDbViaZmq(const std::string &key,
                                 const std::vector<std::pair<std::string, std::string>> &attributes,
                                 const std::string &operation);
        bool writeToAsicDbViaRedis(const std::string &objectType, const std::string &objectId,
                                   const std::string &key,
                                   const std::vector<std::pair<std::string, std::string>> &attributes,
                                   const std::string &operation);
        bool readFromAsicDb(const std::string &objectType, const std::string &objectId,
                            const std::vector<std::pair<std::string, std::string>> &attributes);

        std::string checkSaiResponse(const std::string &operation, int maxAttempts = 10, int timeoutMs = 100);
        bool checkSyncDRunning();

        /*
         * Pick the transport to syncd and, for ZMQ, open the channel.
         *
         * Returns true  - ZMQ channel ready, operations go over zmq.
         *         false - syncd is on the redis queue; use that path.
         * Throws        - ZMQ was chosen but the channel could not be built.
         *                 Fatal by design: falling back would put us on the
         *                 shared GETRESPONSE queue and abort orchagent.
         *
         * The choice follows the same inputs the syncd start script uses, so
         * the two agree on a box the operator never touched: an explicit -z
         * wins, then zmq_enable in the context config, then the CONFIG_DB
         * switches that put -z zmq_sync on syncd's own command line.
         */
        bool initTransport();

        // Pure decision behind initTransport(), kept separate for tests.
        // forcedMode is "" (auto), "redis_sync" or "zmq_sync".
        static bool transportIsZmq(const std::string &forcedMode,
                                   sairedis::context_config_zmq_state_t contextState,
                                   bool configDbWantsZmq);

        // True when CONFIG_DB carries either switch syncd_init_common.sh
        // reads: a DPU switch_type under DEVICE_METADATA, or the
        // SYSTEM_DEFAULTS swss_zmq status.
        bool configDbWantsZmq();

        std::string getSaiObjectType(const std::string &objectName);

    private:
        /*
         * Reject a request syncd would die on. syncd treats an unknown
         * attribute id or a VID it cannot translate as a runtime error, enters
         * shutdown-wait mode and tells orchagent to exit, so a typo in a JSON
         * file restarts the whole data plane. Everything here is checked against
         * SAI metadata and ASIC_DB before anything is sent. Throws
         * std::invalid_argument describing the first problem found.
         */
        void validateRequest(const std::string &objectType,
                             const std::string &objectId,
                             const std::vector<std::pair<std::string, std::string>> &attributes,
                             const std::string &operation);

        // Throws std::invalid_argument unless ASIC_STATE:<type>:<oid> exists.
        void requireObjectInAsicDb(const std::string &objectType, const std::string &objectId,
                                   const std::string &what);

        // The inverse, for entry-type creates whose key the user supplied.
        void requireObjectNotInAsicDb(const std::string &objectType, const std::string &objectId);

        // requireObjectInAsicDb() for a value that may be an OID; non-OIDs and
        // the null OID pass through untouched.
        void requireReferencedOid(const std::string &value, const std::string &what);

        // Throws std::runtime_error until syncd has its switch in VIDTORID.
        void requireSyncdReady();

        // Run the same parsing syncd does over the key and attribute values so a
        // malformed prefix, MAC, enum or list fails here instead of inside syncd.
        void requireDeserializable(const std::string &objectType, const std::string &objectId,
                                   const std::vector<std::pair<std::string, std::string>> &attributes);

        bool isOidFormat(const std::string &str);
        std::string resolveOidValue(const std::string &oidValue);
        // Linked SAI objects support through variable expansion
        std::string resolveVariables(const std::string &value);
        bool isVariableReference(const std::string &value);
        // Helper to build composite key JSON for entry types (route_entry, fdb_entry, neighbor_entry)
        std::string buildCompositeKeyJson(const std::string &objectName, const nlohmann::json &objectData, const std::string &operation);

        std::shared_ptr<swss::DBConnector> m_db;
        std::unique_ptr<SaiConfigManager> m_saiConfigManager;
        std::unordered_map<std::string, std::string> m_objectIdMap;
        // When non-null, operations go over ZMQ to syncd instead of the shared
        // redis queue, so responses never land on the GETRESPONSE queue that
        // orchagent consumes.
        std::shared_ptr<sairedis::Channel> m_channel;
        bool m_noWait;
        bool m_verbose;
        std::string m_forcedMode;        // from -z: "", "redis_sync" or "zmq_sync"
        std::string m_contextConfigFile; // from -x, else the default path
    };
}
