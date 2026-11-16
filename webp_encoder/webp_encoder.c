// 调用顺序为分配输入、写入 RGBA、编码、读取输出，最后释放输入与输出缓冲区
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "webp/encode.h"

static uint8_t *last_output;

__attribute__((export_name("webp_alloc"))) uint8_t *webp_alloc(size_t n) {
  return malloc(n);
}

__attribute__((export_name("webp_free"))) void webp_free(void *p) { free(p); }

__attribute__((export_name("webp_encode_rgba"))) int webp_encode_rgba(
    const uint8_t *rgba, int width, int height, float quality) {
  uint8_t *output = NULL;
  size_t size = WebPEncodeRGBA(rgba, width, height, width * 4, quality, &output);
  last_output = output;
  return (int)size;
}

__attribute__((export_name("webp_get_output"))) uint8_t *webp_get_output(void) {
  return last_output;
}

int main(void) { return 0; }
