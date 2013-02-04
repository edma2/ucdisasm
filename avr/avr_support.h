#ifndef AVR_SUPPORT_H
#define AVR_SUPPORT_H

#include <disasmstream.h>
#include <instruction.h>

/* AVR Disassembly Stream Support */
int disasmstream_avr_init(struct DisasmStream *self);
int disasmstream_avr_close(struct DisasmStream *self);
int disasmstream_avr_read(struct DisasmStream *self, struct instruction *instr);

#endif

