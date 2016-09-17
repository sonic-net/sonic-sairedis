#include "redisclient.h"
#include "swss/logger.h"

namespace swss
{

RedisClient::RedisClient(swss::DBConnector *db):
    m_db(db)
{
}

int64_t RedisClient::del(std::string key)
{
    char *temp;
    int len = redisFormatCommand(&temp, "DEL %s", key.c_str());
    std::string sdel(temp, len);
    free(temp);

    RedisReply r(m_db, sdel, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("DEL operation failed");

    return r.getContext()->integer;
}

int64_t RedisClient::hdel(std::string key, std::string field)
{
    char *temp;
    int len = redisFormatCommand(&temp, "HDEL %s %s", key.c_str(), field.c_str());
    std::string shdel(temp, len);
    free(temp);

    RedisReply r(m_db, shdel, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("HDEL operation failed");

    return r.getContext()->integer;
}

void RedisClient::hset(std::string key, std::string field, std::string value)
{
    char *temp;
    int len = redisFormatCommand(&temp, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    std::string shset(temp, len);
    free(temp);

    RedisReply r(m_db, shset, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("HSET operation failed");
}

void RedisClient::set(std::string key, std::string value)
{
    char *temp;
    int len = redisFormatCommand(&temp, "SET %s %s", key.c_str(), value.c_str());
    std::string sset(temp, len);
    free(temp);

    RedisReply r(m_db, sset, REDIS_REPLY_STATUS, true);

    if (r.getContext()->type != REDIS_REPLY_STATUS)
        throw std::runtime_error("SET operation failed");
}

std::unordered_map<std::string, std::string> RedisClient::hgetall(std::string key)
{
    std::unordered_map<std::string, std::string> map;

    char *temp;
    int len = redisFormatCommand(&temp, "HGETALL %s", key.c_str());

    std::string sincr(temp, len);
    free(temp);

    RedisReply r(m_db, sincr, REDIS_REPLY_ARRAY, true);

    if (r.getContext()->type != REDIS_REPLY_ARRAY)
        throw std::runtime_error("HGETALL operation failed");

    auto ctx = r.getContext();

    for (unsigned int i = 0; i < ctx->elements; i += 2)
        map[std::string(ctx->element[i]->str)] = std::string(ctx->element[i+1]->str);

    return map;
}

std::vector<std::string> RedisClient::keys(std::string key)
{
    std::vector<std::string> list;

    char *temp;
    int len = redisFormatCommand(&temp, "KEYS %s", key.c_str());

    std::string skeys(temp, len);
    free(temp);

    RedisReply r(m_db, skeys, REDIS_REPLY_ARRAY, true);

    if (r.getContext()->type != REDIS_REPLY_ARRAY)
        throw std::runtime_error("KEYS operation failed");

    auto ctx = r.getContext();

    for (unsigned int i = 0; i < ctx->elements; i++)
        list.push_back(ctx->element[i]->str);

    return list;
}

int64_t RedisClient::incr(std::string key)
{
    char *temp;
    int len = redisFormatCommand(&temp, "INCR %s", key.c_str());

    std::string sincr(temp, len);
    free(temp);

    RedisReply r(m_db, sincr, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("INCR command failed");

    return r.getContext()->integer;
}

int64_t RedisClient::decr(std::string key)
{
    char *temp;
    int len = redisFormatCommand(&temp, "DECR %s", key.c_str());

    std::string sdecr(temp, len);
    free(temp);

    RedisReply r(m_db, sdecr, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("DECR command failed");

    return r.getContext()->integer;
}

std::shared_ptr<std::string> RedisClient::get(std::string key)
{
    char *temp;
    int len = redisFormatCommand(&temp, "GET %s", key.c_str());

    std::string sget(temp, len);
    free(temp);

    redisReply *reply;

    redisAppendFormattedCommand(m_db->getContext(), sget.c_str(), sget.length());
    redisGetReply(m_db->getContext(), (void**)&reply);

    if (!reply)
        throw std::runtime_error("GET failed, memory exception");

    if (reply->type == REDIS_REPLY_NIL)
    {
        freeReplyObject(reply);
        return std::shared_ptr<std::string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        std::shared_ptr<std::string> ptr(new std::string(reply->str));
        freeReplyObject(reply);
        return ptr;
    }

    freeReplyObject(reply);

    throw std::runtime_error("GET failed, memory exception");
}

std::shared_ptr<std::string> RedisClient::hget(std::string key, std::string field)
{
    char *temp;
    int len = redisFormatCommand(&temp, "HGET %s %s", key.c_str(), field.c_str());

    std::string shget(temp, len);
    free(temp);

    redisReply *reply;

    redisAppendFormattedCommand(m_db->getContext(), shget.c_str(), shget.length());
    redisGetReply(m_db->getContext(), (void**)&reply);

    if (!reply)
    {
        SWSS_LOG_ERROR("HGET failed, null reply, memory exception: %s: %s", key.c_str(), field.c_str());

        throw std::runtime_error("HGET failed, null reply, memory exception");
    }

    if (reply->type == REDIS_REPLY_NIL)
    {
        freeReplyObject(reply);
        return std::shared_ptr<std::string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        std::shared_ptr<std::string> ptr(new std::string(reply->str));
        freeReplyObject(reply);
        return ptr;
    }

    SWSS_LOG_ERROR("HGET failed, reply-type: %d, %s: %s", reply->type, key.c_str(), field.c_str());

    freeReplyObject(reply);

    throw std::runtime_error("HGET failed, unexpected reply type, memory exception");
}

int64_t RedisClient::rpush(std::string list, std::string item)
{
    char *temp;
    int len = redisFormatCommand(&temp, "RPUSH %s %s", list.c_str(), item.c_str());

    std::string srpush(temp, len);
    free(temp);

    RedisReply r(m_db, srpush, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("RPUSH command failed");

    return r.getContext()->integer;
}

std::shared_ptr<std::string> RedisClient::blpop(std::string list, int timeout)
{
    char *temp;
    int len = redisFormatCommand(&temp, "BLPOP %s %d", list.c_str(), timeout);

    std::string sblpop(temp, len);
    free(temp);

    redisReply *reply;

    redisAppendFormattedCommand(m_db->getContext(), sblpop.c_str(), sblpop.length());
    redisGetReply(m_db->getContext(), (void**)&reply);

    if (!reply)
        throw std::runtime_error("BLPOP failed, memory exception");

    if (reply->type == REDIS_REPLY_NIL)
    {
        freeReplyObject(reply);
        return std::shared_ptr<std::string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        std::shared_ptr<std::string> ptr(new std::string(reply->str));
        freeReplyObject(reply);
        return ptr;
    }

    freeReplyObject(reply);

    throw std::runtime_error("GET failed, memory exception");
}
}
