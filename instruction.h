#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stdint.h>

struct instruction {
    void *data;
    int type;

    uint32_t (*get_address)(struct instruction *);
    unsigned int (*get_width)(struct instruction *);
    unsigned int (*get_num_operands)(struct instruction *);
    unsigned int (*get_opcodes)(struct instruction *, uint8_t *dest);

    int (*get_str_address_label)(struct instruction *, char *dest, int size, int flags);
    int (*get_str_address)(struct instruction *, char *dest, int size, int flags);
    int (*get_str_opcodes)(struct instruction *, char *dest, int size, int flags);
    int (*get_str_mnemonic)(struct instruction *, char *dest, int size, int flags);
    int (*get_str_comment)(struct instruction *, char *dest, int size, int flags);
    int (*get_str_operand)(struct instruction *, char *dest, int size, int index, int flags);

    void (*free)(struct instruction *);
};

enum {
    DISASM_TYPE_INSTRUCTION,
    DISASM_TYPE_DIRECTIVE,
};

#endif
