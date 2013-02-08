#ifndef PIC_SUPPORT_H
#define PIC_SUPPORT_H

#include <disasmstream.h>
#include <instruction.h>

/* PIC Disassembly Stream Supoprt */
int disasmstream_pic_close(struct DisasmStream *self);
int disasmstream_pic_read(struct DisasmStream *self, struct instruction *instr);

/* PIC Baseline Disassembly Stream Support */
int disasmstream_pic_baseline_init(struct DisasmStream *self);
#define disasmstream_pic_baseline_close     disasmstream_pic_close
#define disasmstream_pic_baseline_read      disasmstream_pic_read
/* PIC Midrange Disassembly Stream Support */
int disasmstream_pic_midrange_init(struct DisasmStream *self);
#define disasmstream_pic_midrange_close     disasmstream_pic_close
#define disasmstream_pic_midrange_read      disasmstream_pic_read
/* PIC Midrange Enhanced Disassembly Stream Support */
int disasmstream_pic_midrange_enhanced_init(struct DisasmStream *self);
#define disasmstream_pic_midrange_enhanced_close    disasmstream_pic_close
#define disasmstream_pic_midrange_enhanced_read     disasmstream_pic_read
/* PIC PIC18 Disassembly Stream Support */
int disasmstream_pic_pic18_init(struct DisasmStream *self);
#define disasmstream_pic_pic18_close        disasmstream_pic_close
#define disasmstream_pic_pic18_read         disasmstream_pic_read

#endif

