#pragma once

#include <cstdint>

#include "SwitchConfigContainer.h"

namespace sairedis
{
    /**
     * @brief Per-context ZMQ enablement intent expressed in context_config.json.
     *
     * Three states are required because a context can set zmq_enable to true,
     * set it to false, or omit the key entirely. The omitted case must remain
     * distinguishable from an explicit false so reconciliation can defer to
     * the command-line mode (-z zmq_sync) instead of overriding it.
     */
    typedef enum _context_config_zmq_state_t : uint8_t {

        /**
         * @brief Key absent in JSON. Defer to the command-line mode.
         */
        CONTEXT_CONFIG_ZMQ_EMPTY,

        /**
         * @brief zmq_enable: true. Lock to ZMQ; the command line cannot downgrade.
         */
        CONTEXT_CONFIG_ZMQ_ENABLED,

        /**
         * @brief zmq_enable: false. Lock to Redis; the command line cannot upgrade.
         */
        CONTEXT_CONFIG_ZMQ_DISABLED,

    } context_config_zmq_state_t;

    class ContextConfig
    {
        public:

            ContextConfig(
                    _In_ uint32_t guid,
                    _In_ const std::string& name,
                    _In_ const std::string& dbAsic,
                    _In_ const std::string& dbCounters,
                    _In_ const std::string& dbFlex,
                    _In_ const std::string& dbState);

            virtual ~ContextConfig();

        public:

            void insert(
                    _In_ std::shared_ptr<SwitchConfig> config);

            bool hasConflict(
                    _In_ std::shared_ptr<const ContextConfig> ctx) const;

        private:

            /**
             * @brief Decide whether two contexts sharing a ZMQ endpoint collide.
             *
             * Shared by the zmqEndpoint and zmqNtfEndpoint checks in
             * hasConflict() so the same decision logic is not duplicated.
             */
            static bool zmqEndpointConflict(
                    _In_ const std::string& endpoint,
                    _In_ const std::string& otherEndpoint,
                    _In_ context_config_zmq_state_t zmqEnable,
                    _In_ context_config_zmq_state_t otherZmqEnable,
                    _In_ const char* label);

        public: // TODO to private

            uint32_t m_guid;

            std::string m_name;

            std::string m_dbAsic;

            std::string m_dbCounters;

            std::string m_dbFlex;   // TODO to be removed since only used to subscribe

            std::string m_dbState;

            context_config_zmq_state_t m_zmqEnable;

            std::string m_zmqEndpoint;

            std::string m_zmqNtfEndpoint;

            std::shared_ptr<SwitchConfigContainer> m_scc;
    };
}
