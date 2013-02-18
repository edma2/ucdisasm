#ifndef A8051_SUPPORT_H
#define A8051_SUPPORT_H

#include <disasmstream.h>
#include <instruction.h>

/* 8051 Disassembly Stream Support */
int disasmstream_8051_init(struct DisasmStream *self);
int disasmstream_8051_close(struct DisasmStream *self);
int disasmstream_8051_read(struct DisasmStream *self, struct instruction *instr);

#endif

