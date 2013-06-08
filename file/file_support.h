#include <stdint.h>

/* Atmel Generic Byte Stream Support */
int bytestream_generic_init(struct ByteStream *self);
int bytestream_generic_close(struct ByteStream *self);
int bytestream_generic_read(struct ByteStream *self, uint8_t *data, uint32_t *address);

/* Intel HEX Byte Stream Support */
int bytestream_ihex_init(struct ByteStream *self);
int bytestream_ihex_close(struct ByteStream *self);
int bytestream_ihex_read(struct ByteStream *self, uint8_t *data, uint32_t *address);

/* Motorola S-Record Byte Stream Support */
int bytestream_srecord_init(struct ByteStream *self);
int bytestream_srecord_close(struct ByteStream *self);
int bytestream_srecord_read(struct ByteStream *self, uint8_t *data, uint32_t *address);

/* Binary Byte Stream Support */
int bytestream_binary_init(struct ByteStream *self);
int bytestream_binary_close(struct ByteStream *self);
int bytestream_binary_read(struct ByteStream *self, uint8_t *data, uint32_t *address);

/* ASCII Hex Stream Support */
int bytestream_asciihex_init(struct ByteStream *self);
int bytestream_asciihex_close(struct ByteStream *self);
int bytestream_asciihex_read(struct ByteStream *self, uint8_t *data, uint32_t *address);

/* ELF Stream Support */
int bytestream_elf_init(struct ByteStream *self);
int bytestream_elf_close(struct ByteStream *self);
int bytestream_elf_read(struct ByteStream *self, uint8_t *data, uint32_t *address);
