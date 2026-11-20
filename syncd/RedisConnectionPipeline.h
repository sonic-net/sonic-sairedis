#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/dbconnector.h"
#include "swss/rediscommand.h"

namespace syncd
{
    /**
     * @brief Pipeline of commands on the connection of a DB connector.
     *
     * Commands are appended to the output buffer of the connection and
     * written to redis together when their replies are read, so a batch takes
     * one round trip per COMMANDS_PER_FLUSH commands. Unlike
     * swss::RedisPipeline, which opens a new connection, the commands keep
     * their order with all other commands of the connection. Commands must
     * reply with an integer (HSET, HDEL, DEL).
     */
    class RedisConnectionPipeline
    {
        public:

            /**
             * @brief Commands a pipeline sends before it reads their replies,
             * which bounds the memory and latency of a large batch.
             */
            static constexpr size_t COMMANDS_PER_FLUSH = 10000;

        public:

            RedisConnectionPipeline(
                    _In_ swss::DBConnector& db);

            ~RedisConnectionPipeline();

        public:

            void push(
                    _In_ const swss::RedisCommand& command);

            /**
             * @brief Reads all replies, then throws on the first failed
             * command, so the connection stays in sync when a command fails.
             */
            void flush();

        private:

            redisContext* m_context;

            size_t m_pending;
    };
}
