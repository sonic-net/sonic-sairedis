#pragma once

#include "SwitchConfigContainer.h"

namespace saivs
{
    class ContextConfig
    {
        public:

            ContextConfig(
                    _In_ uint32_t guid,
                    _In_ const std::string& name,
                    _In_ const std::string& dbAsic);

            virtual ~ContextConfig();

        public:

            void insert(
                    _In_ std::shared_ptr<SwitchConfig> config);


        public: // TODO to private

            uint32_t m_guid;

            std::string m_name;

            std::string m_dbAsic; // unit tests channel

            std::shared_ptr<SwitchConfigContainer> m_scc;
    };
}
