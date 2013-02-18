#ifndef A8051_INSTRUCTION_SET_H
#define A8051_INSTRUCTION_SET_H

#include <stdint.h>

/* Index for the .DB raw byte "instruction" */
#define A8051_ISET_INDEX_BYTE     (A8051_TOTAL_INSTRUCTIONS-1)

/* Directive names */
#define A8051_DIRECTIVE_NAME_ORIGIN  ".org"

/* Enumeration for all 8051 Operand Types */
enum {
    OPERAND_NONE,
    /* Implicit operands */
    OPERAND_R, OPERAND_A, OPERAND_AB, OPERAND_C, OPERAND_DPTR,
    OPERAND_IND_R,
    OPERAND_IND_DPTR, OPERAND_IND_A_DPTR, OPERAND_IND_A_PC,
    /* Encoded operands */
    OPERAND_ADDR_DIRECT, OPERAND_ADDR_11, OPERAND_ADDR_16, OPERAND_ADDR_RELATIVE,
    OPERAND_ADDR_DIRECT_SRC, OPERAND_ADDR_DIRECT_DST,
    OPERAND_IMMED, OPERAND_IMMED_16,
    OPERAND_ADDR_BIT, OPERAND_ADDR_NOT_BIT,
};

/* Structure for each entry in the instruction set */
struct a8051InstructionInfo {
    uint8_t opcode;
    char mnemonic[7];
    unsigned int width;
    int numOperands;
    int operandTypes[3];
};

/* Structure for a disassembled instruction */
struct a8051InstructionDisasm {
    uint32_t address;
    uint8_t opcode[3];
    struct a8051InstructionInfo *instructionInfo;
    int32_t operandDisasms[3];
};

/* Structure for a directive */
struct a8051Directive {
    char *name;
    uint32_t value;
};

extern struct a8051InstructionInfo A8051_Instruction_Set[];
extern int A8051_TOTAL_INSTRUCTIONS;

#endif

