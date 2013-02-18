#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <instruction.h>
#include <printstream_file.h>

#include "8051_instruction_set.h"

/* 8051 formats */
#define A8051_FORMAT_OP_REGISTER(fmt)       "R" fmt ""      /* MOV A, R1 */
#define A8051_FORMAT_OP_IND_REGISTER(fmt)   "@R" fmt ""     /* MOV A, @R1 */
#define A8051_FORMAT_OP_ADDR(fmt)           "0" fmt "h"      /* ACALL 01000h */
#define A8051_FORMAT_OP_ADDR_NOT_BIT(fmt)   "/0" fmt "h"     /* ORL C, /025h */
#define A8051_FORMAT_OP_ADDR_RELATIVE(fmt)  "." fmt ""      /* SJMP .-2 */
#define A8051_FORMAT_OP_IMMED(fmt)          "#0" fmt "h"     /* MOV A, #0ffh */
#define A8051_FORMAT_OP_ADDRESS_LABEL(fmt)  "A_" fmt ""     /* AJMP A_0004 */
#define A8051_FORMAT_ADDRESS(fmt)           "" fmt ":"      /* 0004: */
#define A8051_FORMAT_ADDRESS_LABEL(fmt)     "A_" fmt ":"    /* A_0004: */

#if 0
#define A8051_FORMAT_OP_REGISTER(fmt)       "R" fmt ""      /* MOV A, R1 */
#define A8051_FORMAT_OP_IND_REGISTER(fmt)   "@R" fmt ""     /* MOV A, @R1 */
#define A8051_FORMAT_OP_ADDR(fmt)           "0x" fmt ""     /* ACALL 0x1000 */
#define A8051_FORMAT_OP_ADDR_NOT_BIT(fmt)   "/0x" fmt ""    /* ORL C, /0x25 */
#define A8051_FORMAT_OP_ADDR_RELATIVE(fmt)  "." fmt ""      /* SJMP .-2 */
#define A8051_FORMAT_OP_IMMED(fmt)          "#0x" fmt ""    /* MOV A, #0xff */
#define A8051_FORMAT_OP_ADDRESS_LABEL(fmt)  "A_" fmt ""     /* AJMP A_0004 */
#define A8051_FORMAT_ADDRESS(fmt)           "" fmt ":"      /* 0004: */
#define A8051_FORMAT_ADDRESS_LABEL(fmt)     "A_" fmt ":"    /* A_0004: */
#endif

/* Address filed width, e.g. 4 -> 0x0004 */
#define A8051_ADDRESS_WIDTH               4

/* 8051 Instruction Accessor Functions */
uint32_t a8051_instruction_get_address(struct instruction *instr);
unsigned int a8051_instruction_get_width(struct instruction *instr);
unsigned int a8051_instruction_get_num_operands(struct instruction *instr);
unsigned int a8051_instruction_get_opcodes(struct instruction *instr, uint8_t *dest);
int a8051_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags);
int a8051_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags);
int a8051_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags);
int a8051_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
int a8051_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
int a8051_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags);
void a8051_instruction_free(struct instruction *instr);

/* 8051 Directive Accessor Functions */
unsigned int a8051_directive_get_num_operands(struct instruction *instr);
int a8051_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
int a8051_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
void a8051_directive_free(struct instruction *instr);

/******************************************************************************/
/* 8051 Instructions */
/******************************************************************************/

uint32_t a8051_instruction_get_address(struct instruction *instr) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    return instructionDisasm->address;
}

unsigned int a8051_instruction_get_width(struct instruction *instr) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    return instructionDisasm->instructionInfo->width;
}

unsigned int a8051_instruction_get_num_operands(struct instruction *instr) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    return instructionDisasm->instructionInfo->numOperands;
}

unsigned int a8051_instruction_get_opcodes(struct instruction *instr, uint8_t *dest) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    int i;

    for (i = 0; i < instructionDisasm->instructionInfo->width; i++)
        *dest++ = instructionDisasm->opcode[i];

    return instructionDisasm->instructionInfo->width;
}

int a8051_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    return snprintf(dest, size, A8051_FORMAT_ADDRESS_LABEL("%0*x"), A8051_ADDRESS_WIDTH, instructionDisasm->address);
}

int a8051_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    return snprintf(dest, size, A8051_FORMAT_ADDRESS("%*x"), A8051_ADDRESS_WIDTH, instructionDisasm->address);
}

int a8051_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;

    if (instructionDisasm->instructionInfo->width == 1)
        return snprintf(dest, size, "%02x      ", instructionDisasm->opcode[0]);
    else if (instructionDisasm->instructionInfo->width == 2)
        return snprintf(dest, size, "%02x %02x   ", instructionDisasm->opcode[1], instructionDisasm->opcode[0]);
    else if (instructionDisasm->instructionInfo->width == 3)
        return snprintf(dest, size, "%02x %02x %02x", instructionDisasm->opcode[2], instructionDisasm->opcode[1], instructionDisasm->opcode[0]);

    return 0;
}

int a8051_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    return snprintf(dest, size, "%s", instructionDisasm->instructionInfo->mnemonic);
}

int a8051_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;

    if (index < 0 || index > instructionDisasm->instructionInfo->numOperands - 1)
        return 0;

    switch (instructionDisasm->instructionInfo->operandTypes[index]) {
        case OPERAND_R:
            return snprintf(dest, size, A8051_FORMAT_OP_REGISTER("%d"), instructionDisasm->operandDisasms[index]);
        case OPERAND_IND_R:
            return snprintf(dest, size, A8051_FORMAT_OP_IND_REGISTER("%d"), instructionDisasm->operandDisasms[index]);
        case OPERAND_A:
            return snprintf(dest, size, "A");
        case OPERAND_AB:
            return snprintf(dest, size, "AB");
        case OPERAND_C:
            return snprintf(dest, size, "C");
        case OPERAND_DPTR:
            return snprintf(dest, size, "DPTR");
        case OPERAND_IND_DPTR:
            return snprintf(dest, size, "@DPTR");
        case OPERAND_IND_A_DPTR:
            return snprintf(dest, size, "@A+DPTR");
        case OPERAND_IND_A_PC:
            return snprintf(dest, size, "@A+PC");

        case OPERAND_IMMED:
            return snprintf(dest, size, A8051_FORMAT_OP_IMMED("%02x"), instructionDisasm->operandDisasms[index]);
        case OPERAND_IMMED_16:
            return snprintf(dest, size, A8051_FORMAT_OP_IMMED("%04x"), instructionDisasm->operandDisasms[index]);

        case OPERAND_ADDR_DIRECT:
        case OPERAND_ADDR_DIRECT_SRC:
        case OPERAND_ADDR_DIRECT_DST:
        case OPERAND_ADDR_BIT:
            return snprintf(dest, size, A8051_FORMAT_OP_ADDR("%02x"), instructionDisasm->operandDisasms[index]);
        case OPERAND_ADDR_NOT_BIT:
            return snprintf(dest, size, A8051_FORMAT_OP_ADDR_NOT_BIT("%02x"), instructionDisasm->operandDisasms[index]);

        /* Absolute jump / calls */
        case OPERAND_ADDR_11:
        case OPERAND_ADDR_16:
            if (flags & PRINT_FLAG_ASSEMBLY)
                return snprintf(dest, size, A8051_FORMAT_OP_ADDRESS_LABEL("%0*x"), A8051_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index]);
            return snprintf(dest, size, A8051_FORMAT_OP_ADDR("%0*x"), A8051_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index]);

        /* Relative jumps */
        case OPERAND_ADDR_RELATIVE:
            /* If we have address labels turned on, replace the relative
             * address with the appropriate address label */
            if (flags & PRINT_FLAG_ASSEMBLY) {
                return snprintf(dest, size, A8051_FORMAT_OP_ADDRESS_LABEL("%0*x"), A8051_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index] + instructionDisasm->address + instructionDisasm->instructionInfo->width);
            } else {
                if (instructionDisasm->operandDisasms[index] >= 0) {
                    return snprintf(dest, size, A8051_FORMAT_OP_ADDR_RELATIVE("+%d"), instructionDisasm->operandDisasms[index]);
                } else {
                    return snprintf(dest, size, A8051_FORMAT_OP_ADDR_RELATIVE("%d"), instructionDisasm->operandDisasms[index]);
                }
            }
            break;
        default:
            break;
    }

    return 0;
}

int a8051_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags) {
    struct a8051InstructionDisasm *instructionDisasm = (struct a8051InstructionDisasm *)instr->data;
    int i;

    for (i = 0; i < instructionDisasm->instructionInfo->numOperands; i++) {
        if ( instructionDisasm->instructionInfo->operandTypes[i] == OPERAND_ADDR_RELATIVE)
            return snprintf(dest, size, "; " A8051_FORMAT_OP_ADDR("%x"), instructionDisasm->operandDisasms[i] + instructionDisasm->address + instructionDisasm->instructionInfo->width);
    }

    return 0;
}

void a8051_instruction_free(struct instruction *instr) {
    free(instr->data);
    instr->data = NULL;
}

/******************************************************************************/
/* 8051 Directives */
/******************************************************************************/

unsigned int a8051_directive_get_num_operands(struct instruction *instr) {
    return 1;
}

int a8051_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags) {
    struct a8051Directive *directive = (struct a8051Directive *)instr->data;
    return snprintf(dest, size, "%s", directive->name);
}

int a8051_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags) {
    struct a8051Directive *directive = (struct a8051Directive *)instr->data;

    if (strcmp(directive->name, A8051_DIRECTIVE_NAME_ORIGIN) == 0 && index == 0)
        return snprintf(dest, size, A8051_FORMAT_OP_ADDR("%0*x"), A8051_ADDRESS_WIDTH, directive->value);

    return 0;
}

void a8051_directive_free(struct instruction *instr) {
    free(instr->data);
    instr->data = NULL;
}

