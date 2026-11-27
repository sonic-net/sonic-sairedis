#include "RedisConnectionPipeline.h"

#include "swss/logger.h"
#include "swss/redisreply.h"

#include <exception>
#include <new>

using namespace syncd;

constexpr size_t RedisConnectionPipeline::COMMANDS_PER_FLUSH;

RedisConnectionPipeline::RedisConnectionPipeline(
        _In_ swss::DBConnector& db):
    m_context(db.getContext()),
    m_pending(0)
{
    SWSS_LOG_ENTER();

    // empty
}

RedisConnectionPipeline::~RedisConnectionPipeline()
{
    SWSS_LOG_ENTER();

    // when an exception interrupted the batch, read the replies of the
    // commands already appended, so the connection does not answer its next
    // commands with them

    for (; m_pending; m_pending--)
    {
        redisReply* reply = nullptr;

        if (redisGetReply(m_context, (void**)&reply) != REDIS_OK)
        {
            break;
        }

        freeReplyObject(reply);
    }
}

void RedisConnectionPipeline::push(
        _In_ const swss::RedisCommand& command)
{
    SWSS_LOG_ENTER();

    if (command.appendTo(m_context) != REDIS_OK)
    {
        // out of memory is the only append error of hiredis

        throw std::bad_alloc();
    }

    m_pending++;

    if (m_pending >= COMMANDS_PER_FLUSH)
    {
        flush();
    }
}

void RedisConnectionPipeline::flush()
{
    SWSS_LOG_ENTER();

    std::exception_ptr error;

    while (m_pending)
    {
        redisReply* reply = nullptr;

        if (redisGetReply(m_context, (void**)&reply) != REDIS_OK)
        {
            m_pending = 0; // connection is broken, no more replies

            throw swss::RedisError("Failed to redisGetReply in RedisConnectionPipeline::flush", m_context);
        }

        m_pending--;

        swss::RedisReply r(reply);

        try
        {
            r.checkReplyType(REDIS_REPLY_INTEGER);
        }
        catch (const std::exception&)
        {
            if (!error)
            {
                error = std::current_exception();
            }
        }
    }

    if (error)
    {
        std::rethrow_exception(error);
    }
}
