#include "RedisCommandArgs.h"

#include "swss/logger.h"

using namespace syncd;

RedisCommandArgs::RedisCommandArgs(
        _In_ size_t count)
{
    SWSS_LOG_ENTER();

    m_argv.reserve(count);
    m_argvlen.reserve(count);
}

void RedisCommandArgs::add(
        _In_ const char* arg,
        _In_ size_t len)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons, called per argument

    m_argv.push_back(arg);
    m_argvlen.push_back(len);
}

void RedisCommandArgs::add(
        _In_ const std::string& arg)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons, called per argument

    add(arg.c_str(), arg.size());
}

void RedisCommandArgs::format(
        _Out_ swss::RedisCommand& command)
{
    SWSS_LOG_ENTER();

    command.formatArgv((int)m_argv.size(), m_argv.data(), m_argvlen.data());
}
