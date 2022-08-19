/*
 *
 * $Id: brcm_pai_log.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef __PAI_LOG_H_
#define __PAI_LOG_H_

#include "sai.h"

/* Temporary setting it default (SAI_LOG_LEVEL_DEBUG). This will set later by the switch attribute*/
#ifdef PAI_SYSLOG_ENABLE
#define PAI_LOG(api_id,loglevel,format,...)                                                                                        \
if (loglevel >= _adapter_log_level[api_id]) {                                                                                      \
    if (loglevel >= SAI_LOG_LEVEL_INFO && loglevel <= SAI_LOG_LEVEL_WARN) {                                                        \
        syslog(brcm_sai_to_syslog_log_level_get(loglevel), "%s " format "\n",__func__,##__VA_ARGS__);                              \
    } else {                                                                                                                       \
        syslog(brcm_sai_to_syslog_log_level_get(loglevel), "%s:%d %s: " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    }                                                                                                                              \
}
#else
#define PAI_LOG(api_id,loglevel,format,...)                                                     \
if (loglevel >= _adapter_log_level[api_id]) {                                                   \
    if (loglevel >= SAI_LOG_LEVEL_INFO && loglevel <= SAI_LOG_LEVEL_WARN) {                     \
        fprintf(stderr, "%s " format "\n",__func__,##__VA_ARGS__);                              \
    } else {                                                                                    \
        fprintf(stderr, "%s:%d %s: " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    }                                                                                           \
}
#endif

/*
 * Helper macros.
 */
#define PAI_LOG_ENTER(api_id)                PAI_LOG(api_id, SAI_LOG_LEVEL_DEBUG,    ":> enter");
#define PAI_LOG_DEBUG(api_id,format,...)     PAI_LOG(api_id, SAI_LOG_LEVEL_DEBUG,    ":- " format, ##__VA_ARGS__)
#define PAI_LOG_INFO(api_id,format,...)      PAI_LOG(api_id, SAI_LOG_LEVEL_INFO,     ":- " format, ##__VA_ARGS__)
#define PAI_LOG_NOTICE(api_id,format,...)    PAI_LOG(api_id, SAI_LOG_LEVEL_NOTICE,   ":- " format, ##__VA_ARGS__)
#define PAI_LOG_WARN(api_id,format,...)      PAI_LOG(api_id, SAI_LOG_LEVEL_WARN,     ":- " format, ##__VA_ARGS__)
#define PAI_LOG_ERROR(api_id,format,...)     PAI_LOG(api_id, SAI_LOG_LEVEL_ERROR,    ":- " format, ##__VA_ARGS__)
#define PAI_LOG_CRITICAL(api_id,format,...)  PAI_LOG(api_id, SAI_LOG_LEVEL_CRITICAL, ":- " format, ##__VA_ARGS__)
#define PAI_LOG_EXIT(api_id)                 PAI_LOG(api_id, SAI_LOG_LEVEL_DEBUG,    ":< exit");

#endif /** __PAI_LOG_H_ */
