#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/rediscommand.h"

#include <string>
#include <vector>

namespace syncd
{
    /**
     * @brief Arguments of a redis command, formatted from strings that
     * outlive it, without copying them as
     * swss::RedisCommand::format(const std::vector<std::string>&) does.
     */
    class RedisCommandArgs
    {
        public:

            RedisCommandArgs(
                    _In_ size_t count);

        public:

            void add(
                    _In_ const char* arg,
                    _In_ size_t len);

            void add(
                    _In_ const std::string& arg);

            void format(
                    _Out_ swss::RedisCommand& command);

        private:

            std::vector<const char*> m_argv;

            std::vector<size_t> m_argvlen;
    };
}
