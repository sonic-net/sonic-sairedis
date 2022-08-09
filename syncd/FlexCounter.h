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

namespace syncd
{
    class BaseCounterContext
    {
    public:
        BaseCounterContext(const std::string &name);
        void addPlugins(
            _In_ const std::vector<std::string>& shaStrings);

        bool hasPlugin() const {return !m_plugins.empty();}

        void removePlugins() {m_plugins.clear();}

        virtual void addObject(
                _In_ sai_object_id_t vid,
                _In_ sai_object_id_t rid,
                _In_ const std::vector<std::string> &idStrings,
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
        std::set<std::string> m_plugins;

    public:
        bool always_check_supported_counters = false;
        bool use_sai_stats_capa_query = true;
        bool use_sai_stats_ext = false;
        bool double_confirm_supported_counters = false;
    };
    class FlexCounter
    {
        private:
            FlexCounter(const FlexCounter&) = delete;

        public:
            FlexCounter(
                    _In_ const std::string& instanceId,
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ const std::string& dbCounters);

            virtual ~FlexCounter();

        public:
            void addCounterPlugin(
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void removeCounterPlugins();

            void addCounter(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid,
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
                    _In_ const std::string &name);

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
    };
}
