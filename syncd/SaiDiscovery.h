#pragma once

#include "meta/SaiInterface.h"

#include "AttrVersionChecker.h"

#include <memory>
#include <set>
#include <map>
#include <unordered_map>

#include "swss/logger.h"

namespace syncd
{

    enum SaiDiscoveryFlags
    {
        None = 0,
        SkipDefaultEmptyAttributes = 1 << 0,
    };

    inline SaiDiscoveryFlags operator|(SaiDiscoveryFlags a, SaiDiscoveryFlags b)
    {
        SWSS_LOG_ENTER();

        return static_cast<SaiDiscoveryFlags>(static_cast<int>(a) | static_cast<int>(b));
    }

    class SaiDiscovery
    {
        public:

            typedef std::unordered_map<sai_object_id_t, std::unordered_map<sai_attr_id_t, sai_object_id_t>> DefaultOidMap;

        public:

            SaiDiscovery(
                    _In_ std::shared_ptr<sairedis::SaiInterface> sai,
                    _In_ SaiDiscoveryFlags flags = SaiDiscoveryFlags::None);

            virtual ~SaiDiscovery();

        public:

            std::set<sai_object_id_t> discover(
                    _In_ sai_object_id_t rid);

            std::set<sai_object_id_t> discover(
                    _In_ const sai_object_id_t* rids,
                    _In_ size_t count);

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

            SaiDiscoveryFlags m_flags;

            DefaultOidMap m_defaultOidMap;

            AttrVersionChecker m_attrVersionChecker;
    };
}
