#include <stddef.h>
__attribute__((used, externally_visible)) void *
memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (n--)
    *d++ = *s++;
  return dst;
}
__attribute__((used, externally_visible)) void *memset(void *dst, int v,
                                                       size_t n) {
  unsigned char *d = dst;
  while (n--)
    *d++ = (unsigned char)v;
  return dst;
}
