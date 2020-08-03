#include "MetadataLogger.h"

extern "C"{
#include "saimetadata.h"
}

#include "swss/logger.h"

#include <stdarg.h>

#define MAX_BUFFER_LENGTH (0x1000)

using namespace syncd;

static void sai_meta_log_syncd(
        _In_ sai_log_level_t logLevel,
        _In_ const char *file,
        _In_ int line,
        _In_ const char *func,
        _In_ const char *format,
        ...)
    __attribute__ ((format (printf, 5, 6)));

static void sai_meta_log_syncd(
        _In_ sai_log_level_t logLevel,
        _In_ const char *file,
        _In_ int line,
        _In_ const char *func,
        _In_ const char *format,
        ...)
{
    // SWSS_LOG_ENTER() is omitted since this is logging for metadata

    char buffer[MAX_BUFFER_LENGTH];

    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, MAX_BUFFER_LENGTH, format, ap);
    va_end(ap);

    swss::Logger::Priority priority = swss::Logger::SWSS_NOTICE;

    switch (logLevel)
    {
        case SAI_LOG_LEVEL_DEBUG:
            priority = swss::Logger::SWSS_DEBUG;
            break;
        case SAI_LOG_LEVEL_INFO:
            priority = swss::Logger::SWSS_INFO;
            break;
        case SAI_LOG_LEVEL_ERROR:
            priority = swss::Logger::SWSS_ERROR;
            fprintf(stderr, "ERROR: %s: %s\n", func, buffer);
            break;
        case SAI_LOG_LEVEL_WARN:
            priority = swss::Logger::SWSS_WARN;
            fprintf(stderr, "WARN: %s: %s\n", func, buffer);
            break;
        case SAI_LOG_LEVEL_CRITICAL:
            priority = swss::Logger::SWSS_CRIT;
            break;

        default:
            priority = swss::Logger::SWSS_NOTICE;
            break;
    }

    swss::Logger::getInstance().write(priority, ":- %s: %s", func, buffer);
}

void MetadataLogger::initialize()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("initializeing metadata log function");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
    sai_metadata_log = &sai_meta_log_syncd;
#pragma GCC diagnostic pop
}
