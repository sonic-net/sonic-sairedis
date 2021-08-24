#pragma once

#include "ContextConfig.h"

namespace saivs
{
    class Context
    {
        public:

            Context(
                    _In_ std::shared_ptr<ContextConfig> contextConfig);

            virtual ~Context();

        public:

            std::shared_ptr<ContextConfig> getContextConfig() const;

        private:

            std::shared_ptr<ContextConfig> m_contextConfig;
    };
}
