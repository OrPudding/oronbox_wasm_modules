#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "quickjs.h"
#include "quickjs-libc.h"

static JSRuntime *g_rt = NULL;
static JSContext *g_ctx = NULL;

typedef struct {
  char *channel;
  char *payload;
} QueuedMessage;

static QueuedMessage *g_messages = NULL;
static int g_message_count = 0;
static int g_message_capacity = 0;
static char *g_last_error = NULL;
typedef struct ModuleSource {
  char *name;
  char *source;
  struct ModuleSource *next;
} ModuleSource;
static ModuleSource *g_modules = NULL;

static JSModuleDef *load_module(JSContext *ctx, const char *name, void *opaque) {
  for (ModuleSource *module = g_modules; module; module = module->next) {
    if (strcmp(module->name, name)) continue;
    JSValue compiled = JS_Eval(ctx, module->source, strlen(module->source), name,
                              JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled)) return NULL;
    JSModuleDef *result = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    return result;
  }
  JS_ThrowReferenceError(ctx, "Module is not in the plugin package: %s", name);
  return NULL;
}
static int64_t g_deadline_ms = 0;

static int64_t monotonic_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int interrupt_execution(JSContext *ctx, void *opaque) {
  return g_deadline_ms && monotonic_ms() >= g_deadline_ms;
}

static void capture_error(JSContext *ctx) {
  JSValue error = JS_GetException(ctx);
  JSValue stack = JS_GetPropertyStr(ctx, error, "stack");
  const char *message = JS_ToCString(ctx, error);
  const char *trace = JS_IsUndefined(stack) ? NULL : JS_ToCString(ctx, stack);
  free(g_last_error);
  size_t size = (message ? strlen(message) : 0) + (trace ? strlen(trace) : 0) + 2;
  g_last_error = malloc(size);
  if (g_last_error) snprintf(g_last_error, size, "%s\n%s", message ? message : "JavaScript error", trace ? trace : "");
  if (message) JS_FreeCString(ctx, message);
  if (trace) JS_FreeCString(ctx, trace);
  JS_FreeValue(ctx, stack);
  JS_FreeValue(ctx, error);
}

static JSValue js_send_message(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) return JS_UNDEFINED;
  const char *channel = JS_ToCString(ctx, argv[0]);
  const char *payload = JS_ToCString(ctx, argv[1]);
  if (channel && payload) {
    if (g_message_count == g_message_capacity) {
      int capacity = g_message_capacity ? g_message_capacity * 2 : 32;
      QueuedMessage *messages = realloc(g_messages, capacity * sizeof(*messages));
      if (!messages) {
        JS_FreeCString(ctx, channel);
        JS_FreeCString(ctx, payload);
        return JS_ThrowOutOfMemory(ctx);
      }
      g_messages = messages;
      g_message_capacity = capacity;
    }
    char *channel_copy = strdup(channel);
    char *payload_copy = strdup(payload);
    if (!channel_copy || !payload_copy) {
      free(channel_copy);
      free(payload_copy);
      JS_FreeCString(ctx, channel);
      JS_FreeCString(ctx, payload);
      return JS_ThrowOutOfMemory(ctx);
    }
    g_messages[g_message_count++] = (QueuedMessage){channel_copy, payload_copy};
  }
  if (channel) JS_FreeCString(ctx, channel);
  if (payload) JS_FreeCString(ctx, payload);
  return JS_UNDEFINED;
}

__attribute__((export_name("qjs_alloc")))
void *qjs_alloc(size_t size) {
  return malloc(size);
}

__attribute__((export_name("qjs_free")))
void qjs_free(void *ptr) {
  free(ptr);
}

__attribute__((export_name("qjs_init")))
int qjs_init(void) {
  if (g_rt) return 0;
  g_rt = JS_NewRuntime();
  if (!g_rt) return -1;
  JS_SetMemoryLimit(g_rt, 64 * 1024 * 1024);
  JS_SetMaxStackSize(g_rt, 1024 * 1024);
  JS_SetInterruptHandler(g_rt, interrupt_execution, NULL);
  JS_SetModuleLoaderFunc(g_rt, NULL, load_module, NULL);
  g_ctx = JS_NewContext(g_rt);
  if (!g_ctx) {
    JS_FreeRuntime(g_rt);
    g_rt = NULL;
    return -1;
  }
  js_std_add_helpers(g_ctx, 0, NULL);

  JSValue global_obj = JS_GetGlobalObject(g_ctx);
  JS_SetPropertyStr(g_ctx, global_obj, "sendMessage",
                    JS_NewCFunction(g_ctx, js_send_message, "sendMessage", 2));
  JS_FreeValue(g_ctx, global_obj);
  return 0;
}

__attribute__((export_name("qjs_eval")))
int qjs_eval(const char *code, size_t len, const char *filename) {
  if (!g_ctx) return -1;
  g_deadline_ms = monotonic_ms() + 10000;
  JSValue val = JS_Eval(g_ctx, code, len, filename ? filename : "<eval>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(val)) {
    capture_error(g_ctx);
    JS_FreeValue(g_ctx, val);
    return -1;
  }
  JS_FreeValue(g_ctx, val);
  return 0;
}

__attribute__((export_name("qjs_add_module")))
int qjs_add_module(const char *name, const char *source) {
  ModuleSource *module = calloc(1, sizeof(*module));
  if (!module) return -1;
  module->name = strdup(name);
  module->source = strdup(source);
  if (!module->name || !module->source) {
    free(module->name);
    free(module->source);
    free(module);
    return -1;
  }
  module->next = g_modules;
  g_modules = module;
  return 0;
}

__attribute__((export_name("qjs_eval_entry")))
int qjs_eval_entry(const char *code, size_t len, const char *filename) {
  if (!g_ctx) return -1;
  g_deadline_ms = monotonic_ms() + 10000;
  size_t name_len = strlen(filename);
  int is_module = (name_len >= 4 && !strcmp(filename + name_len - 4, ".mjs")) || JS_DetectModule(code, len);
  JSValue value = JS_Eval(g_ctx, code, len, filename, is_module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(value)) {
    capture_error(g_ctx);
    return -1;
  }
  JSValue global = JS_GetGlobalObject(g_ctx);
  JS_SetPropertyStr(g_ctx, global, "__obModuleResult", value);
  JS_FreeValue(g_ctx, global);
  return 0;
}

__attribute__((export_name("qjs_loop")))
int qjs_loop(void) {
  if (!g_rt) return 0;
  g_deadline_ms = monotonic_ms() + 10000;
  JSContext *pctx;
  int ret;
  while ((ret = JS_ExecutePendingJob(g_rt, &pctx)) > 0) {}
  if (ret < 0) capture_error(pctx);
  return ret;
}

__attribute__((export_name("qjs_string_length")))
size_t qjs_string_length(const char *value) { return value ? strlen(value) : 0; }

__attribute__((export_name("qjs_get_last_error")))
const char *qjs_get_last_error(void) { return g_last_error; }

__attribute__((export_name("qjs_get_message_count")))
int qjs_get_message_count(void) {
  return g_message_count;
}

__attribute__((export_name("qjs_get_message_channel")))
const char *qjs_get_message_channel(int index) {
  if (index >= 0 && index < g_message_count) {
    return g_messages[index].channel;
  }
  return NULL;
}

__attribute__((export_name("qjs_get_message_payload")))
const char *qjs_get_message_payload(int index) {
  if (index >= 0 && index < g_message_count) {
    return g_messages[index].payload;
  }
  return NULL;
}

__attribute__((export_name("qjs_clear_messages")))
void qjs_clear_messages(void) {
  for (int i = 0; i < g_message_count; i++) {
    free(g_messages[i].channel);
    free(g_messages[i].payload);
  }
  g_message_count = 0;
}

__attribute__((export_name("qjs_destroy")))
void qjs_destroy(void) {
  qjs_clear_messages();
  free(g_messages);
  g_messages = NULL;
  g_message_capacity = 0;
  free(g_last_error);
  g_last_error = NULL;
  while (g_modules) {
    ModuleSource *module = g_modules;
    g_modules = module->next;
    free(module->name);
    free(module->source);
    free(module);
  }
  if (g_ctx) {
    JS_FreeContext(g_ctx);
    g_ctx = NULL;
  }
  if (g_rt) {
    JS_FreeRuntime(g_rt);
    g_rt = NULL;
  }
}
