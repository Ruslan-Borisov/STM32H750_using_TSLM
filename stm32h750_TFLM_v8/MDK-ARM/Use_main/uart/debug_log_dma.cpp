/* Copyright 2023 The TensorFlow Authors. All Rights Reserved. */
#include "tensorflow/lite/micro/debug_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "tensorflow/lite/micro/cortex_m_generic/debug_log_callback.h"

/* ============== œŒƒ Àﬁ◊¿≈Ã Õ¿ÿ «¿√ŒÀŒ¬Œ  ============== */
#include "uart_use.h"

#ifndef TF_LITE_STRIP_ERROR_STRINGS
#include <stdio.h>
#endif

static DebugLogCallback debug_log_callback = nullptr;

namespace {

void InvokeDebugLogCallback(const char* s) {
  if (debug_log_callback != nullptr) {
    debug_log_callback(s);
  } else {
    /* ============== »—œŒÀ‹«”≈Ã Õ¿ÿ” ‘”Õ ÷»ﬁ ============== */
    Debug_LogString(s);
  }
}

}  // namespace

void RegisterDebugLogCallback(void (*cb)(const char* s)) {
  debug_log_callback = cb;
}

void DebugLog(const char* format, va_list args) {
#ifndef TF_LITE_STRIP_ERROR_STRINGS
  constexpr int kMaxLogLen = 256;
  char log_buffer[kMaxLogLen];

  vsnprintf(log_buffer, kMaxLogLen, format, args);
  InvokeDebugLogCallback(log_buffer);
#endif
}

#ifndef TF_LITE_STRIP_ERROR_STRINGS
int DebugVsnprintf(char* buffer, size_t buf_size, const char* format,
                   va_list vlist) {
  return vsnprintf(buffer, buf_size, format, vlist);
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif