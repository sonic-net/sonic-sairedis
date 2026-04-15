#include "SaiVppLog.h"

#include "swss/logger.h"

#include <cstdio>
#include <cstdarg>

#define SAIVPP_LOG_BUF_SIZE 512

extern "C" {

void saivpp_log_error(const char *func, const char *fmt, ...)
{
    char buf[SAIVPP_LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    swss::Logger::getInstance().write(swss::Logger::SWSS_ERROR, ":- %s: %s", func, buf);
}

void saivpp_log_warn(const char *func, const char *fmt, ...)
{
    char buf[SAIVPP_LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    swss::Logger::getInstance().write(swss::Logger::SWSS_WARN, ":- %s: %s", func, buf);
}

void saivpp_log_notice(const char *func, const char *fmt, ...)
{
    char buf[SAIVPP_LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    swss::Logger::getInstance().write(swss::Logger::SWSS_NOTICE, ":- %s: %s", func, buf);
}

void saivpp_log_info(const char *func, const char *fmt, ...)
{
    char buf[SAIVPP_LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    swss::Logger::getInstance().write(swss::Logger::SWSS_INFO, ":- %s: %s", func, buf);
}

void saivpp_log_debug(const char *func, const char *fmt, ...)
{
    char buf[SAIVPP_LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG, ":- %s: %s", func, buf);
}

}
