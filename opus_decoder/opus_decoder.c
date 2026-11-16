#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "include/opus_multistream.h"

__attribute__((export_name("opus_alloc")))
void *wrapper_opus_alloc(size_t size) {
  return malloc(size);
}

__attribute__((export_name("opus_free")))
void wrapper_opus_free(void *ptr) {
  free(ptr);
}

__attribute__((export_name("opus_decoder_create")))
OpusMSDecoder *wrapper_opus_decoder_create(
    int32_t Fs,
    int channels,
    int streams,
    int coupled_streams,
    const unsigned char *mapping,
    int dummy1,
    int dummy2) {
  int error = 0;
  return opus_multistream_decoder_create(Fs, channels, streams, coupled_streams, mapping, &error);
}

__attribute__((export_name("opus_decode_float")))
int wrapper_opus_decode_float(
    OpusMSDecoder *st,
    const unsigned char *data,
    int32_t len,
    float *pcm) {
  return opus_multistream_decode_float(st, data, len, pcm, 5760, 0);
}

__attribute__((export_name("opus_decoder_destroy")))
void wrapper_opus_decoder_destroy(OpusMSDecoder *st) {
  if (st) {
    opus_multistream_decoder_destroy(st);
  }
}
