#pragma once

extern "C" {
#include "sai.h"
}

#include "FlexCounterSyncState.h"
#include "meta/SaiInterface.h"

#include "swss/table.h"

#include <vector>
#include <set>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <map>
#include <string>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <atomic>
#include <functional>

// Placeholder type for attributes not requiring data allocation
// Used as default template parameter for simple attributes
struct NoAttrData {
    // Empty struct - minimal footprint, compiler optimizes away
};

// Holds data storage for SAI PORT attribute API calls
struct PortPhyAttributeData {
    std::vector<sai_port_lane_latch_status_t> rxSignalDetectData;
    std::vector<sai_port_lane_latch_status_t> fecAlignmentLockData;
    std::vector<sai_port_snr_values_t> rxSnrData;
};

// Holds data storage for SAI PORT_SERDES attribute API calls
struct PortPhySerdesAttributeData {
    // For SAI_PORT_SERDES_ATTR_RX_VGA
    std::vector<uint32_t> rxVgaData;

    // For SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST
    std::vector<std::vector<int32_t>> txFirTapsData;
    std::vector<sai_s32_list_t> txFirTapsList;
};

namespace syncd
{
    class BaseCounterContext
    {
    public:
        // Called with the group mutex held at safe object/chunk boundaries.
        using PollCheckpoint = std::function<bool(swss::Table&)>;

        struct ObjectRegistration
        {
            uint64_t epoch;

            sai_object_id_t rid;
        };

        using ObjectRegistrationMap = std::map<std::pair<std::string, sai_object_id_t>, ObjectRegistration>;

        // Shared lifetime, not independent synchronization: accesses still
        // require the owning FlexCounter group's mutex.
        struct PollingState
        {
            ObjectRegistrationMap registrations;

            bool yielded = false;

            sai_stats_mode_t statsMode = SAI_STATS_MODE_READ;
        };

        struct ObjectRid
        {
            sai_object_id_t rid;

            bool consistent;
        };

        using ObjectRidMap = std::map<sai_object_id_t, ObjectRid>;

        BaseCounterContext(
                _In_ const std::string& name,
                _In_ const std::string& instance,
                _In_ std::shared_ptr<const PollingState> pollingState);

        void clearSamples(
                _In_ sai_stats_mode_t statsMode);

        void addPlugins(
            _In_ const std::vector<std::string>& shaStrings);

        void setNoDoubleCheckBulkCapability(
            _In_ bool);

        virtual void setBulkChunkSize(
            _In_ uint32_t bulkChunkSize);

        virtual void setBulkChunkSizePerPrefix(
            _In_ const std::string& bulkChunkSizePerPrefix);

        bool hasPlugin() const {return !m_plugins.empty();}

        void removePlugins() {m_plugins.clear();}

        virtual void addObjectWithCounterGroups(
                _In_ sai_object_id_t vid,
                _In_ sai_object_id_t rid,
                _In_ const std::vector<std::string> &idStrings,
                _In_ const std::string &per_object_stats_mode) = 0;

        virtual void addObject(
                _In_ sai_object_id_t vid,
                _In_ sai_object_id_t rid,
                _In_ const std::vector<std::string> &idStrings,
                _In_ const std::string &per_object_stats_mode) = 0;

        virtual void bulkAddObjectWithCounterGroups(
                _In_ const std::vector<sai_object_id_t>& vids,
                _In_ const std::vector<sai_object_id_t>& rids,
                _In_ const std::vector<std::string>& idStrings,
                _In_ const std::string &per_object_stats_mode) = 0;

        virtual void bulkAddObject(
                _In_ const std::vector<sai_object_id_t>& vids,
                _In_ const std::vector<sai_object_id_t>& rids,
                _In_ const std::vector<std::string>& idStrings,
                _In_ const std::string &per_object_stats_mode) = 0;

        virtual void removeObject(
                _In_ sai_object_id_t vid) = 0;

        virtual void collectData(
                _In_ swss::Table &countersTable,
                _In_ const PollCheckpoint& checkpoint) = 0;

        virtual void runPlugin(
                _In_ swss::DBConnector& counters_db,
                _In_ const std::vector<std::string>& argv) = 0;

        virtual bool hasObject() const = 0;

        // Report actual enrolled SDK keys, including all scalar/bulk fragments.
        // Missing VIDs are omitted. Called only while the group mutex is held.
        virtual ObjectRidMap getObjectRids(
                _In_ const std::vector<sai_object_id_t>& vids) const = 0;

        // Only ordinary single-object stats enrollment removes/replaces old keys.
        virtual bool supportsRidReplacement() const;

    protected:
        bool isPollRegistrationCurrent(
                _In_ sai_object_id_t vid) const;

        void markSampled(
                _In_ sai_object_id_t vid);

        bool isSampleReady(
                _In_ sai_object_id_t vid) const;

        std::string m_name;
        std::string m_instanceId;
        std::set<std::string> m_plugins;
        std::string m_bulkChunkSizePerPrefix;
        std::map<std::pair<sai_object_id_t, sai_object_id_t>, uint32_t> m_failedPolls;

        const std::shared_ptr<const PollingState> m_pollingState;

        std::map<sai_object_id_t, uint64_t> m_pollEpochs;

        std::map<sai_object_id_t, uint64_t> m_sampledEpochs;

        sai_stats_mode_t m_pollStatsMode = SAI_STATS_MODE_READ;

    public:
        bool always_check_supported_counters = false;
        bool use_sai_stats_capa_query = true;
        bool use_sai_stats_ext = false;
        bool double_confirm_supported_counters = false;
        bool no_double_check_bulk_capability = false;
        bool dont_clear_support_counter  = false;
        uint32_t default_bulk_chunk_size = 0;
    };
    class FlexCounter
    {
        private:
            FlexCounter(const FlexCounter&) = delete;

        public:
            FlexCounter(
                    _In_ const std::string& instanceId,
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ const std::string& dbCounters,
                    _In_ const bool noDoubleCheckBulkCapability=false);

            virtual ~FlexCounter();

        public:
            void addCounterPlugin(
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void removeCounterPlugins();

            void addCounter(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid,
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void bulkAddCounter(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<sai_object_id_t>& vids,
                    _In_ const std::vector<sai_object_id_t>& rids,
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void removeCounter(
                    _In_ sai_object_id_t vid);

            bool isEmpty();

            bool isDiscarded();

        private:

            void setPollInterval(
                    _In_ uint32_t pollInterval);

            void setSecondaryPollFactor(
                    _In_ uint32_t secondaryPollFactor);

            void setStatus(
                    _In_ const std::string& status);

            void setStatsMode(
                    _In_ const std::string& mode);

        private:
            bool allIdsEmpty() const;

            bool allPluginsEmpty() const;

        private: // remove counter
            void removeDataFromCountersDB(
                    _In_ sai_object_id_t vid,
                    _In_ const std::string &ratePrefix);

        private:
            bool hasSameRidRegistration(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid) const;

            void recordRegistration(
                    _In_ const std::string& name,
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid);

            void enrollCounterObjects(
                    _In_ const std::string& name,
                    _In_ const std::vector<sai_object_id_t>& vids,
                    _In_ const std::vector<sai_object_id_t>& rids,
                    _In_ const std::vector<std::string>& idStrings,
                    _In_ const std::string& statsMode,
                    _In_ bool bulk,
                    _In_ bool perPortDiscovery);

            std::shared_ptr<BaseCounterContext> getCounterContext(
                    _In_ const std::string &name);

            std::shared_ptr<BaseCounterContext> createCounterContext(
                    _In_ const std::string &name,
                    _In_ const std::string &instance);

            void removeCounterContext(
                    _In_ const std::string &name);

            bool hasCounterContext(
                    _In_ const std::string &name) const;

            void collectCounters(
                    _In_ swss::Table &countersTable,
                    _Inout_ std::unique_lock<std::mutex>& lock);

            void runPlugins(
                    _In_ swss::DBConnector& db,
                    _In_ uint32_t pollInterval,
                    _In_ uint32_t secondaryPollFactor,
                    _In_ int64_t cycleStartUs,
                    _In_ int64_t collectUs);

            void startFlexCounterThread();

            void endFlexCounterThread();

            void flexCounterThreadRunFunction();

        private:
            void waitPoll();

            void notifyPoll();

            void notifySleep();

        private:
            std::atomic<bool> m_runFlexCounterThread;

            std::shared_ptr<std::thread> m_flexCounterThread;

            std::mutex m_mtxSleep;

            std::condition_variable m_cvSleep;

            FlexCounterSyncState m_syncState;

            // Preserve configuration notifications even before wait_for().
            std::atomic<uint64_t> m_sleepGeneration{0};

            std::condition_variable m_pollCond;

            bool m_readyToPoll;

            uint32_t m_pollInterval;

            uint32_t m_secondaryPollFactor;

            std::string m_instanceId;

            bool m_enable;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            std::string m_dbCounters;

            bool m_isTcpConn;

            bool m_isDiscarded;

            const std::shared_ptr<BaseCounterContext::PollingState> m_pollingState =
                std::make_shared<BaseCounterContext::PollingState>();

            uint64_t m_nextObjectEpoch = 0;

            std::map<std::string, std::shared_ptr<BaseCounterContext>> m_counterContext;

            bool m_noDoubleCheckBulkCapability;

            static const std::map<std::string, std::string> m_plugIn2CounterType;

            static const std::map<std::tuple<sai_object_type_t, std::string>, std::string> m_objectTypeField2CounterType;
    };
}
