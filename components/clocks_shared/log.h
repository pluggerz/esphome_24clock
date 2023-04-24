
#ifdef ESP8266
#include "esphome/core/log.h"

using esphome::esp_log_printf_;

#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define STRINGIFY_IMPL(x) #x
#define DBGI "[" __FILE__ ":" STRINGIFY(__LINE__)

#define LOGE(tag, what, ...) esph_log_e(tag, what DBGI, ##__VA_ARGS__)
#define LOGW(tag, what, ...) esph_log_w(tag, what DBGI, ##__VA_ARGS__)
#define LOGI(tag, what, ...) esph_log_i(tag, what DBGI, ##__VA_ARGS__)
#define LOGD(tag, what, ...) esph_log_d(tag, what DBGI, ##__VA_ARGS__)
#define LOGCONFIG(tag, what, ...) \
  esph_log_logconfig(tag, what DBGI, ##__VA_ARGS__)
#define LOGV(tag, what, ...) esph_log_v(tag, what DBGI, ##__VA_ARGS__)
#define LOGVV(tag, what, args, ...) esph_log_vv(tag, what DBGI, ##__VA_ARGS__)

#else
#define LOGVV(...)
#define LOGV(...)
#define LOGD(...)
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)

#define ESP_LOGVV(...)
#define ESP_LOGV(...)
#define ESP_LOGD(...)
#define ESP_LOGI(...)
#define ESP_LOGW(...)
#define ESP_LOGE(...)

#endif

#ifdef ESP8266

#define spit_info(ch, value) LOGD(TAG, "spit_info(%c > %d)", ch, value);
#define spit_debug(ch, value) LOGD(TAG, "spit_debug(%c > %d)", ch, value);

#else

#ifndef PERFORMER_DEBUG
#error please define ! PERFORMER_DEBUG=true or PERFORMER_DEBUG=false
#endif

void spit_info_impl(char ch, int value);
#define spit_info(ch, value) spit_info_impl(ch, value)

#if PERFORMER_DEBUG == true
#warning spit_debug -> impl
void spit_debug_impl(char ch, int value);
#define spit_debug(ch, value) spit_debug_impl(ch, value)
#else
#warning spit_debug -> null
#define spit_debug(ch, value)
#endif

#endif