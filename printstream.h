#ifndef PRINTSTREAM_H
#define PRINTSTREAM_H

#include <stdio.h>
#include <disasmstream.h>
#include <instruction.h>
#include <stream_error.h>

struct PrintStream {
    /* Input stream */
    struct DisasmStream *in;
    /* Stream state */
    void *state;
    /* Error */
    char *error;

    /* Init function */
    int (*stream_init)(struct PrintStream *self, int flags);
    /* Close function */
    int (*stream_close)(struct PrintStream *self);
    /* Output function */
    int (*stream_read)(struct PrintStream *self, FILE *out);
};

#endif

