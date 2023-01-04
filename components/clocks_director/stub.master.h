#pragma once

#include "Arduino.h"

#define NUMBER_OF_STEPS 720

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
