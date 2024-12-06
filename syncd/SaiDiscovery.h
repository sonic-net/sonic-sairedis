#pragma once

#include "meta/SaiInterface.h"

#include <memory>
#include <set>
#include <map>
#include <unordered_map>

namespace syncd
{
    class SaiDiscovery
    {
        public:

            typedef std::unordered_map<sai_object_id_t, std::unordered_map<sai_attr_id_t, sai_object_id_t>> DefaultOidMap;

        public:

            SaiDiscovery(
                    _In_ std::shared_ptr<sairedis::SaiInterface> sai);

            virtual ~SaiDiscovery();

        public:

            std::set<sai_object_id_t> discover(
                    _In_ sai_object_id_t rid);

            const DefaultOidMap& getDefaultOidMap() const;

        private:

            /**
             * @brief Discover objects on the switch.
             *
             * Method will query recursively all OID attributes (oid and list) on
             * the given object.
             *
             * This method should be called only once inside constructor right
             * after switch has been created to obtain actual ASIC view.
             *
             * @param rid Object to discover other objects.
             * @param processed Set of already processed objects. This set will be
             * updated every time new object ID is discovered.
             */
            void discover(
                    _In_ sai_object_id_t rid,
                    _Inout_ std::set<sai_object_id_t> &processed);

            void setApiLogLevel(
                    _In_ sai_log_level_t logLevel);

            void setApiLogLevel(
                    _In_ const std::map<sai_api_t, sai_log_level_t>& levels);

            std::map<sai_api_t, sai_log_level_t> getApiLogLevel();

        private:

            std::shared_ptr<sairedis::SaiInterface> m_sai;

            DefaultOidMap m_defaultOidMap;
    };
}
