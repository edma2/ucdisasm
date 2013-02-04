#include <stdint.h>
#include <stdio.h>

struct bytestream_debug_state {
    uint8_t *data;
    uint32_t *address;
    unsigned int len;
    int index;
};

int bytestream_debug_init(struct ByteStream *self);
int bytestream_debug_close(struct ByteStream *self);
int bytestream_debug_read(struct ByteStream *self, uint8_t *data, uint32_t *address);

