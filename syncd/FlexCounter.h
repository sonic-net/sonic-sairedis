#pragma once

extern "C" {
#include "sai.h"
}

#include "meta/SaiInterface.h"

#include "swss/table.h"

#include <vector>
#include <set>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <type_traits>

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

namespace syncd
{
    class BaseCounterContext
    {
    public:
        BaseCounterContext(const std::string &name, const std::string &instance);
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

        virtual void addObject(
                _In_ sai_object_id_t vid,
                _In_ sai_object_id_t rid,
                _In_ const std::vector<std::string> &idStrings,
                _In_ const std::string &per_object_stats_mode) = 0;

        virtual void bulkAddObject(
                _In_ const std::vector<sai_object_id_t>& vids,
                _In_ const std::vector<sai_object_id_t>& rids,
                _In_ const std::vector<std::string>& idStrings,
                _In_ const std::string &per_object_stats_mode) = 0;

        virtual void removeObject(
                _In_ sai_object_id_t vid) = 0;

        virtual void collectData(
                _In_ swss::Table &countersTable) = 0;

        virtual void runPlugin(
                _In_ swss::DBConnector& counters_db,
                _In_ const std::vector<std::string>& argv) = 0;

        virtual bool hasObject() const = 0;

    protected:
        std::string m_name;
        std::string m_instanceId;
        std::set<std::string> m_plugins;
        std::string m_bulkChunkSizePerPrefix;

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
                    _In_ swss::Table &countersTable);

            void runPlugins(
                    _In_ swss::DBConnector& db);

            void startFlexCounterThread();

            void endFlexCounterThread();

            void flexCounterThreadRunFunction();

        private:
            void waitPoll();

            void notifyPoll();

        private:
            bool m_runFlexCounterThread;

            std::shared_ptr<std::thread> m_flexCounterThread;

            std::mutex m_mtxSleep;

            std::condition_variable m_cvSleep;

            std::mutex m_mtx;

            std::condition_variable m_pollCond;

            bool m_readyToPoll;

            uint32_t m_pollInterval;

            std::string m_instanceId;

            sai_stats_mode_t m_statsMode;

            bool m_enable;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            std::string m_dbCounters;

            bool m_isDiscarded;

            std::map<std::string, std::shared_ptr<BaseCounterContext>> m_counterContext;

            bool m_noDoubleCheckBulkCapability;

            static const std::map<std::string, std::string> m_plugIn2CounterType;

            static const std::map<std::tuple<sai_object_type_t, std::string>, std::string> m_objectTypeField2CounterType;
    };
}
