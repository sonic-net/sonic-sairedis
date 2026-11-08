#ifndef __SAI_VPP_LOG_H__
#define __SAI_VPP_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

void saivpp_log_error(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void saivpp_log_warn(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void saivpp_log_notice(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void saivpp_log_info(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void saivpp_log_debug(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#define SAIVPP_ERROR(fmt, ...)  saivpp_log_error(__func__, fmt, ##__VA_ARGS__)
#define SAIVPP_WARN(fmt, ...)   saivpp_log_warn(__func__, fmt, ##__VA_ARGS__)
#define SAIVPP_NOTICE(fmt, ...) saivpp_log_notice(__func__, fmt, ##__VA_ARGS__)
#define SAIVPP_INFO(fmt, ...)   saivpp_log_info(__func__, fmt, ##__VA_ARGS__)
#define SAIVPP_DEBUG(fmt, ...)  saivpp_log_debug(__func__, fmt, ##__VA_ARGS__)

#endif /* __SAI_VPP_LOG_H__ */
