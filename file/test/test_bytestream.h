#include <stdio.h>
#include <stdint.h>

/* Byte Stream File Test */
int test_bytestream(FILE *in, int (*stream_init)(struct ByteStream *self), int (*stream_close)(struct ByteStream *self), int (*stream_read)(struct ByteStream *self, uint8_t *data, uint32_t *address));
