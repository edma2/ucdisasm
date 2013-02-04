#ifndef PRINTSTREAM_FILE_H
#define PRINTSTREAM_FILE_H

#include <stdio.h>
#include <printstream.h>

/* Print Stream Option Flags */
enum {
    PRINT_FLAG_ASSEMBLY                = (1<<0),
    PRINT_FLAG_ADDRESSES               = (1<<1),
    PRINT_FLAG_DESTINATION_COMMENT     = (1<<2),
    PRINT_FLAG_DATA_HEX                = (1<<3),
    PRINT_FLAG_DATA_BIN                = (1<<4),
    PRINT_FLAG_DATA_DEC                = (1<<5),
    PRINT_FLAG_OPCODES                 = (1<<6),
};

/* Print Stream Support */
int printstream_file_init(struct PrintStream *self, int flags);
int printstream_file_close(struct PrintStream *self);
int printstream_file_read(struct PrintStream *self, FILE *out);

#endif

