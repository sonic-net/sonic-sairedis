#pragma once

#include "FlexCounter.h"

#include <functional>

namespace syncd
{
    class FlexCounterManager
    {
        public:

            FlexCounterManager(
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ const std::string& dbCounters,
                    _In_ const std::string& supportingBulkInstances);

            virtual ~FlexCounterManager() = default;

        public:

            std::shared_ptr<FlexCounter> getInstance(
                    _In_ const std::string& instanceId);

            void removeInstance(
                    _In_ const std::string& instanceId);

            void removeAllCounters();

            void removeCounterPlugins(
                    _In_ const std::string& instanceId);

            void addCounterPlugin(
                    _In_ const std::string& instanceId,
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void addCounter(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid,
                    _In_ const std::string& instanceId,
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void bulkAddCounter(
                _In_ const std::vector<sai_object_id_t> &vids,
                _In_ const std::vector<sai_object_id_t> &rids,
                _In_ const std::string& instanceId,
                _In_ const std::vector<swss::FieldValueTuple>& values);

            void removeCounter(
                    _In_ sai_object_id_t vid,
                    _In_ const std::string& instanceId);

            /** Set resolver for VID->RID when RID is 0 (read from Redis VIDTORID). Syncd sets this after translator is created. */
            void setVidToRidResolver(
                    _In_ std::function<bool(sai_object_id_t vid, sai_object_id_t& rid)> resolver);

        private:

                std::map<std::string, std::shared_ptr<FlexCounter>> m_flexCounters;

                std::mutex m_mutex;

                std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

                std::string m_dbCounters;

                std::string m_supportingBulkGroups;

                std::function<bool(sai_object_id_t vid, sai_object_id_t& rid)> m_vidToRidResolver;
    };
}

